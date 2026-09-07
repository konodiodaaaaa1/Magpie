#include "pch.h"
#include "FrameGuidanceDiagnostics.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"

namespace Magpie {

namespace {

constexpr char DIAGNOSTIC_HLSL[] = R"(
Texture2D<float4> Guidance : register(t0);
RWTexture2D<float4> Output : register(u0);

cbuffer Params : register(b0) {
    uint2 Extent;
    uint Kind;
    float Gain;
};

float3 HsvToRgb(float3 c) {
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

[numthreads(8, 8, 1)]
void Visualize(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= Extent)) return;
    float4 guide = Guidance.Load(int3(tid.xy, 0));
    float3 color;
    if (Kind == 0) {
        float2 motion = guide.xy * Gain;
        float magnitude = length(motion);
        float hue = frac(atan2(-motion.y, motion.x) / 6.28318530718 + 1.0);
        color = HsvToRgb(float3(hue, saturate(magnitude),
            saturate(0.18 + magnitude)));
    } else {
        float confidence = saturate(guide.x);
        color = lerp(float3(1.0, 0.0, 0.0), confidence.xxx,
            smoothstep(0.2, 0.65, confidence));
    }
    Output[tid.xy] = float4(color, 1.0);
}
)";

}

FrameGuidanceRequirements
FrameGuidanceDiagnostics::GetFrameGuidanceRequirements() const noexcept {
	FrameGuidanceRequirements result{ .zero = true };
	result.Add(MotionVectorRequest::Nvidia(
		NvidiaOpticalFlowQuality::Balanced));
	return result;
}

EffectParameterApplyMode FrameGuidanceDiagnostics::GetParameterApplyMode(
	std::string_view parameterName
) const noexcept {
	return parameterName == "gain"
		? EffectParameterApplyMode::Live
		: EffectParameterApplyMode::RestartRequired;
}

EffectParameterRestartReason FrameGuidanceDiagnostics::GetParameterRestartReason(
	std::string_view /*parameterName*/
) const noexcept {
	return EffectParameterRestartReason::NativeBackend;
}

bool FrameGuidanceDiagnostics::ApplyLiveParameters(
	const EffectOption& option,
	std::span<const std::string> parameterNames
) noexcept {
	if (std::ranges::any_of(parameterNames, [](const std::string& name) {
		return name != "gain";
	})) {
		return false;
	}
	const auto it = option.parameters.find("gain");
	if (it == option.parameters.end() || !std::isfinite(it->second)) {
		return false;
	}
	_settings.gain = std::clamp(it->second, 0.005f, 1.0f);
	return true;
}

bool FrameGuidanceDiagnostics::Initialize(
	DeviceResources& resources,
	ID3D11Texture2D* input,
	ID3D11Texture2D* output,
	const FrameGuidanceDiagnosticSettings& settings
) noexcept {
	_device = resources.GetD3DDevice();
	_context = resources.GetD3DDC();
	_settings = settings;
	winrt::com_ptr<ID3DBlob> blob;
	if (!DirectXHelper::CompileComputeShader(
		DIAGNOSTIC_HLSL, "Visualize", blob.put(),
		"Diagnostics/FrameGuidance.hlsl") ||
		FAILED(_device->CreateComputeShader(
			blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, _shader.put()))) {
		return false;
	}
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = 16,
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
	};
	return SUCCEEDED(_device->CreateBuffer(&desc, nullptr, _params.put())) &&
		Resize(resources, input, output);
}

bool FrameGuidanceDiagnostics::Resize(
	DeviceResources&,
	ID3D11Texture2D*,
	ID3D11Texture2D* output
) noexcept {
	if (!output) return false;
	D3D11_TEXTURE2D_DESC desc{};
	output->GetDesc(&desc);
	return (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0;
}

bool FrameGuidanceDiagnostics::Draw(
	const NativeEffectDrawContext& draw
) noexcept {
	const FrameGuidanceView& view = draw.frameGuidance;
	ID3D11Texture2D* texture = nullptr;
	switch (_settings.kind) {
	case FrameGuidanceDiagnosticKind::Motion:
		texture = view.motion.texture;
		break;
	case FrameGuidanceDiagnosticKind::Confidence:
		texture = view.confidence.texture;
		break;
	}
	if (!texture || !draw.output) return false;

	D3D11_TEXTURE2D_DESC outputDesc{};
	draw.output->GetDesc(&outputDesc);
	winrt::com_ptr<ID3D11ShaderResourceView> srv;
	winrt::com_ptr<ID3D11UnorderedAccessView> uav;
	if (FAILED(_device->CreateShaderResourceView(texture, nullptr, srv.put())) ||
		FAILED(_device->CreateUnorderedAccessView(
			draw.output, nullptr, uav.put()))) return false;
	struct Params {
		uint32_t width, height, kind;
		float gain;
	};
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(_context->Map(
		_params.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
	*static_cast<Params*>(mapped.pData) = {
		outputDesc.Width, outputDesc.Height, static_cast<uint32_t>(_settings.kind),
		_settings.gain
	};
	_context->Unmap(_params.get(), 0);
	ID3D11ShaderResourceView* srvs[]{ srv.get() };
	ID3D11UnorderedAccessView* uavs[]{ uav.get() };
	ID3D11Buffer* buffers[]{ _params.get() };
	_context->CSSetShader(_shader.get(), nullptr, 0);
	_context->CSSetShaderResources(0, 1, srvs);
	_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	_context->CSSetConstantBuffers(0, 1, buffers);
	_context->Dispatch(
		(outputDesc.Width + 7) / 8, (outputDesc.Height + 7) / 8, 1);
	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11UnorderedAccessView* nullUav = nullptr;
	_context->CSSetShaderResources(0, 1, &nullSrv);
	_context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
	_context->CSSetShader(nullptr, nullptr, 0);
	return true;
}

}
