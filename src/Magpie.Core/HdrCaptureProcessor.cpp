#include "pch.h"
#include "HdrCaptureProcessor.h"

#include "BackendDescriptorStore.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"

namespace Magpie {

HdrCaptureProcessor::~HdrCaptureProcessor() noexcept {
	ResetForResize();
}

namespace {
constexpr char HLSL[] = R"(
cbuffer Transform : register(b0) {
    float exposure;
    float inverseExposure;
    float sdrWhiteScale;
    float hdrPeakNits;
    float shoulder;
    float sdrWhiteNits;
    uint inputTransfer;
    uint outputTransfer;
    uint preserveAlpha;
};
Texture2D<float4> sourceTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

float DecodeSrgb(float value) {
    return value <= 0.04045 ? value / 12.92 : pow(max((value + 0.055) / 1.055, 0.0), 2.4);
}
float DecodePq(float value) {
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;
    float p = pow(max(saturate(value), 0.0), 1.0 / m2);
    return pow(max(p - c1, 0.0) / max(c2 - c3 * p, 1e-6), 1.0 / m1) * 10000.0;
}
float DecodeTransfer(float value, uint transfer) {
    if (transfer == 2) return DecodeSrgb(value);
    if (transfer == 3) return DecodePq(value);
    if (transfer == 4) {
        const float a = 0.17883277;
        const float b = 1.0 - 4.0 * a;
        const float c = 0.5 - a * log(4.0 * a);
        return value <= 0.5 ? (value * value) / 3.0 : (exp((value - c) / a) + b) / 12.0;
    }
    return value;
}
float3 ToCanonical(float3 value) {
    float3 decoded = float3(DecodeTransfer(value.r, inputTransfer), DecodeTransfer(value.g, inputTransfer), DecodeTransfer(value.b, inputTransfer));
    // Canonical storage is linear scRGB. WGC FP16 already uses this contract.
    // PQ decodes to absolute nits and is converted to scRGB (80 nits == 1.0).
    // SDR/HLG relative values are embedded at their declared reference white.
    if (inputTransfer == 3) decoded /= 80.0;
    else if (inputTransfer != 1) decoded *= sdrWhiteNits / 80.0;
    return decoded * exposure;
}
[numthreads(8, 8, 1)]
void Main(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    outputTexture.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    float4 value = sourceTexture.Load(int3(id.xy, 0));
    float3 canonical = ToCanonical(value.rgb);
    outputTexture[id.xy] = float4(canonical, preserveAlpha != 0 ? value.a : 1.0);
}
)";
}

bool HdrCaptureProcessor::Initialize(
	DeviceResources& deviceResources,
	BackendDescriptorStore& descriptorStore
) noexcept {
	_deviceResources = &deviceResources;
	_descriptorStore = &descriptorStore;
	winrt::com_ptr<ID3DBlob> blob;
	if (!DirectXHelper::CompileComputeShader(HLSL, "Main", blob.put(), "HdrCaptureProcessor", nullptr, {}, true)) {
		return false;
	}
	HRESULT hr = deviceResources.GetD3DDevice()->CreateComputeShader(
		blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, _shader.put());
	if (FAILED(hr)) {
		Logger::Get().ComError("创建 HDR 捕获处理 Compute Shader 失败", hr);
		return false;
	}
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = 48,
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
	};
	hr = deviceResources.GetD3DDevice()->CreateBuffer(&desc, nullptr, _constants.put());
	return SUCCEEDED(hr);
}

bool HdrCaptureProcessor::_EnsureResources(const D3D11_TEXTURE2D_DESC& sourceDesc) noexcept {
	if (_canonical) {
		D3D11_TEXTURE2D_DESC current{};
		_canonical->GetDesc(&current);
		if (current.Width == sourceDesc.Width && current.Height == sourceDesc.Height) return true;
		if (_descriptorStore) {
			_descriptorStore->RemoveCache(_canonical.get());
		}
	}
	_canonical = DirectXHelper::CreateTexture2D(
		_deviceResources->GetD3DDevice(), DXGI_FORMAT_R16G16B16A16_FLOAT,
		sourceDesc.Width, sourceDesc.Height,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	if (!_canonical) return false;
	_canonicalUav = _descriptorStore->GetUnorderedAccessView(_canonical.get());
	return _canonicalUav != nullptr;
}

bool HdrCaptureProcessor::Process(ID3D11Texture2D* source, const HdrFrameMetadata& metadata) noexcept {
	_lastAssumption.clear();
	if (!_deviceResources || !_shader || !source || metadata.width == 0 || metadata.height == 0) {
		Logger::Get().Error(fmt::format(
			"HDR capture processor rejected frame: device={} shader={} source={} size={}x{}",
			_deviceResources != nullptr, _shader != nullptr, source != nullptr,
			metadata.width, metadata.height));
		return false;
	}
	HdrFrameMetadata normalizedMetadata = metadata;
	if (!normalizedMetadata.color.IsValid()) {
		normalizedMetadata.color.dxgiColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		normalizedMetadata.color.primaries = HdrColorPrimaries::Rec709;
		normalizedMetadata.color.transfer = HdrTransferFunction::SRGB;
		normalizedMetadata.color.range = HdrColorRange::Full;
        normalizedMetadata.color.referenceWhiteNits = 80.0f;
        normalizedMetadata.color.sdrWhiteNits = 80.0f;
		normalizedMetadata.color.displayPeakNits = 1000.0f;
		normalizedMetadata.color.preExposure = 1.0f;
		normalizedMetadata.color.isInferred = true;
		normalizedMetadata.valid = true;
		_lastAssumption = "Capture API did not provide a valid source color description; using default SDR.";
	} else if (normalizedMetadata.color.isInferred) {
		_lastAssumption = "Source color description inferred from the target monitor; capture API metadata was unavailable.";
	}
	if (!normalizedMetadata.IsValid()) {
		Logger::Get().Error("HDR capture processor metadata is invalid after normalization");
		return false;
	}
	D3D11_TEXTURE2D_DESC sourceDesc{};
	source->GetDesc(&sourceDesc);
	if (sourceDesc.Width != normalizedMetadata.width || sourceDesc.Height != normalizedMetadata.height) {
		Logger::Get().Error(fmt::format(
			"HDR capture processor size mismatch: texture={}x{} metadata={}x{}",
			sourceDesc.Width, sourceDesc.Height,
			normalizedMetadata.width, normalizedMetadata.height));
		return false;
	}
	if (!_EnsureResources(sourceDesc)) {
		Logger::Get().Error(fmt::format(
			"HDR capture processor canonical resource creation failed: format={} size={}x{}",
			static_cast<uint32_t>(sourceDesc.Format), sourceDesc.Width, sourceDesc.Height));
		return false;
	}
	_sourceSrv = _descriptorStore->GetShaderResourceView(source);
	if (!_sourceSrv) {
		Logger::Get().Error("HDR capture processor source SRV creation failed");
		return false;
	}
	const HdrTransformParameters parameters = HdrColorTransform::ForFrame(normalizedMetadata.color);
	const HdrTransformConstants constants = HdrColorTransform::PrepareConstants(
		parameters, normalizedMetadata.color.transfer, HdrTransferFunction::Linear);
	struct CaptureConstants {
		float exposure;
		float inverseExposure;
		float sdrWhiteScale;
		float hdrPeakNits;
		float shoulder;
        float sdrWhiteNits;
		uint32_t inputTransfer;
		uint32_t outputTransfer;
		uint32_t preserveAlpha;
		uint32_t _padding[3]{};
	} captureConstants{
		constants.exposure, constants.inverseExposure, constants.sdrWhiteScale,
        constants.hdrPeakNits, constants.shoulder, parameters.sdrWhiteNits,
		constants.inputTransfer, constants.outputTransfer,
		parameters.preserveAlpha ? 1u : 0u
	};
	D3D11_MAPPED_SUBRESOURCE mapped{};
	auto* context = _deviceResources->GetD3DDC();
	if (FAILED(context->Map(_constants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		Logger::Get().Error("HDR capture processor constant-buffer map failed");
		return false;
	}
	memcpy(mapped.pData, &captureConstants, sizeof(captureConstants));
	context->Unmap(_constants.get(), 0);
	context->CSSetShader(_shader.get(), nullptr, 0);
	ID3D11Buffer* constantsBuffer = _constants.get();
	context->CSSetConstantBuffers(0, 1, &constantsBuffer);
	context->CSSetShaderResources(0, 1, &_sourceSrv);
	context->CSSetUnorderedAccessViews(0, 1, &_canonicalUav, nullptr);
	context->Dispatch((sourceDesc.Width + 7) / 8, (sourceDesc.Height + 7) / 8, 1);
	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11UnorderedAccessView* nullUav = nullptr;
	ID3D11Buffer* nullBuffer = nullptr;
	context->CSSetShaderResources(0, 1, &nullSrv);
	context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
	context->CSSetConstantBuffers(0, 1, &nullBuffer);
	context->CSSetShader(nullptr, nullptr, 0);
	_metadata = normalizedMetadata;
	_metadata.sourceFormat = sourceDesc.Format;
	_metadata.stage = HdrFrameStage::CanonicalInput;
	return true;
}

bool HdrCaptureProcessor::Prepare(
	ID3D11Texture2D* sourceTexture,
	DXGI_FORMAT sourceFormat,
	const ColorDescription& sourceColorDescription
) noexcept {
	if (!_deviceResources || !sourceTexture) return false;
	D3D11_TEXTURE2D_DESC sourceDesc{};
	sourceTexture->GetDesc(&sourceDesc);
	if (!_EnsureResources(sourceDesc)) return false;
	const float clearValue[4]{};
	_deviceResources->GetD3DDC()->ClearUnorderedAccessViewFloat(_canonicalUav, clearValue);
	_metadata = {};
	_metadata.width = sourceDesc.Width;
	_metadata.height = sourceDesc.Height;
	_metadata.sourceFormat = sourceFormat != DXGI_FORMAT_UNKNOWN ? sourceFormat : sourceDesc.Format;
	_metadata.color = sourceColorDescription;
	if (!_metadata.color.IsValid()) {
		_metadata.color.dxgiColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		_metadata.color.primaries = HdrColorPrimaries::Rec709;
		_metadata.color.transfer = HdrTransferFunction::SRGB;
		_metadata.color.range = HdrColorRange::Full;
        _metadata.color.referenceWhiteNits = 80.0f;
        _metadata.color.sdrWhiteNits = 80.0f;
		_metadata.color.displayPeakNits = 1000.0f;
		_metadata.color.preExposure = 1.0f;
		_metadata.color.isInferred = true;
		_lastAssumption = "Capture API did not provide a source color description; using default SDR.";
	} else if (_metadata.color.isInferred) {
		_lastAssumption = "Source color description inferred from the target monitor; capture API metadata was unavailable.";
	}
	_metadata.valid = true;
	_metadata.stage = HdrFrameStage::CanonicalInput;
	return true;
}

bool HdrCaptureProcessor::Process(
	ID3D11Texture2D* sourceTexture,
	DXGI_FORMAT sourceFormat,
	const ColorDescription& sourceColorDescription
) noexcept {
	if (!sourceTexture) {
		return false;
	}

	D3D11_TEXTURE2D_DESC sourceDesc{};
	sourceTexture->GetDesc(&sourceDesc);

	HdrFrameMetadata metadata{};
	metadata.frameId = 0;
	metadata.width = sourceDesc.Width;
	metadata.height = sourceDesc.Height;
	metadata.sourceFormat = sourceFormat != DXGI_FORMAT_UNKNOWN ? sourceFormat : sourceDesc.Format;
	metadata.valid = true;

	_lastAssumption.clear();
	if (sourceColorDescription.IsValid()) {
		metadata.color = sourceColorDescription;
	} else {
		metadata.color.dxgiColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		metadata.color.primaries = HdrColorPrimaries::Rec709;
		metadata.color.transfer = HdrTransferFunction::SRGB;
		metadata.color.range = HdrColorRange::Full;
        metadata.color.referenceWhiteNits = 80.0f;
        metadata.color.sdrWhiteNits = 80.0f;
		metadata.color.displayPeakNits = 1000.0f;
		metadata.color.isSceneReferred = false;
		metadata.color.isPreExposed = false;
		metadata.color.isInferred = true;
		metadata.color.preExposure = 1.0f;
		_lastAssumption = "Capture API did not provide a source color description; using default SDR "
			"(Rec.709/sRGB, 80 nits reference white, 1000 nits display peak).";
	}

	return Process(sourceTexture, metadata);
}

void HdrCaptureProcessor::ResetForResize() noexcept {
	if (_descriptorStore && _canonical) {
		_descriptorStore->RemoveCache(_canonical.get());
	}
	_canonical = nullptr;
	_canonicalUav = nullptr;
	_sourceSrv = nullptr;
	_metadata = {};
	_lastAssumption.clear();
}

}
