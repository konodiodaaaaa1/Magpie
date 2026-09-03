#include "pch.h"
#include "FrameGuidanceService.h"
#include "DeviceResources.h"
#include "Logger.h"
#include "DirectXHelper.h"

namespace Magpie {

namespace {

constexpr char ADAPT_GUIDANCE_HLSL[] = R"(
Texture2D<float2> SourceMotion : register(t0);
Texture2D<float> SourceDepth : register(t1);
Texture2D<float> SourceConfidence : register(t2);
RWTexture2D<float2> TargetMotion : register(u0);
RWTexture2D<float> TargetDepth : register(u1);
RWTexture2D<float> TargetConfidence : register(u2);

cbuffer Params : register(b0) {
    uint2 SourceExtent;
    uint2 TargetExtent;
    float2 MotionScale;
    float2 Padding;
};

float2 SourcePosition(uint2 targetPixel) {
    return (float2(targetPixel) + 0.5) *
        (float2(SourceExtent) / float2(TargetExtent)) - 0.5;
}

int2 ClampSource(int2 pixel) {
    return clamp(pixel, int2(0, 0), int2(SourceExtent) - 1);
}

float4 LoadGuide(int2 pixel) {
    pixel = ClampSource(pixel);
    return float4(
        SourceMotion.Load(int3(pixel, 0)),
        SourceDepth.Load(int3(pixel, 0)),
        SourceConfidence.Load(int3(pixel, 0)));
}

[numthreads(8, 8, 1)]
void Adapt(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= TargetExtent)) return;
    float2 sourcePosition = SourcePosition(tid.xy);
    int2 base = int2(floor(sourcePosition));
    float2 fraction = frac(sourcePosition);
    float4 top = lerp(LoadGuide(base), LoadGuide(base + int2(1, 0)), fraction.x);
    float4 bottom = lerp(
        LoadGuide(base + int2(0, 1)),
        LoadGuide(base + int2(1, 1)), fraction.x);
    float4 guide = lerp(top, bottom, fraction.y);
    TargetMotion[tid.xy] = guide.xy * MotionScale;
    TargetDepth[tid.xy] = guide.z;
    TargetConfidence[tid.xy] = saturate(guide.w);
}
)";

FrameGuidanceRegion ScaleRegion(
	FrameGuidanceRegion region,
	FrameGuidanceExtent source,
	FrameGuidanceExtent target
) noexcept {
	const auto scaleFloor = [](uint32_t value, uint32_t numerator,
		uint32_t denominator) noexcept {
		return static_cast<uint32_t>(
			(uint64_t(value) * numerator) / denominator);
	};
	const auto scaleCeil = [](uint32_t value, uint32_t numerator,
		uint32_t denominator) noexcept {
		return static_cast<uint32_t>(
			(uint64_t(value) * numerator + denominator - 1) / denominator);
	};
	const uint32_t left = std::min(
		scaleFloor(region.x, target.width, source.width), target.width);
	const uint32_t top = std::min(
		scaleFloor(region.y, target.height, source.height), target.height);
	const uint32_t right = std::min(
		scaleCeil(region.x + region.width, target.width, source.width),
		target.width);
	const uint32_t bottom = std::min(
		scaleCeil(region.y + region.height, target.height, source.height),
		target.height);
	return { left, top, right - left, bottom - top };
}

FrameGuidanceMetadata AdaptMetadata(
	const FrameGuidanceMetadata& source,
	FrameGuidanceExtent targetExtent,
	ID3D11Fence* fence,
	uint64_t fenceValue,
	bool forceReset = false
) noexcept {
	FrameGuidanceMetadata result = source;
	result.validRegion = ScaleRegion(
		source.validRegion, source.sourceExtent, targetExtent);
	result.sourceExtent = targetExtent;
	result.sync = { fence, fenceValue };
	result.requiresHistoryReset = source.requiresHistoryReset || forceReset;
	return result;
}

FrameGuidanceMetadata MakeTargetZeroMetadata(
	FrameGuidanceFrameId frameId,
	FrameGuidanceExtent extent,
	ID3D11Fence* fence,
	uint64_t fenceValue,
	FrameGuidanceResetReason reason,
	bool requiresHistoryReset
) noexcept {
	return {
		.frameId = frameId,
		.sourceExtent = extent,
		.validRegion = FrameGuidanceRegion::Full(extent),
		.sync = { fence, fenceValue },
		.resetReason = reason,
		.valid = true,
		.isZero = true,
		.requiresHistoryReset = requiresHistoryReset
	};
}

}

struct FrameGuidanceService::AdapterCache {
	struct Entry {
		FrameGuidanceExtent extent{};
		std::array<winrt::com_ptr<ID3D11Texture2D>, 3> adaptedTextures;
		std::array<winrt::com_ptr<ID3D11Texture2D>, 3> zeroTextures;
		std::array<winrt::com_ptr<ID3D11UnorderedAccessView>, 3> adaptedUavs;
		std::array<winrt::com_ptr<ID3D11UnorderedAccessView>, 3> zeroUavs;
		std::array<ID3D11Texture2D*, 3> sourceTextures{};
		std::array<winrt::com_ptr<ID3D11ShaderResourceView>, 3> sourceSrvs;
		FrameGuidanceView produced{};
		FrameGuidanceView zero{};
		FrameGuidanceFrameId frameId = 0;
		bool hasFrame = false;
		bool fallbackActive = false;
	};

	bool Initialize(DeviceResources& resources) noexcept {
		device = resources.GetD3DDevice();
		context = resources.GetD3DDC();
		winrt::com_ptr<ID3DBlob> blob;
		if (!device || !context || !DirectXHelper::CompileComputeShader(
			ADAPT_GUIDANCE_HLSL, "Adapt", blob.put(),
			"FrameGuidance/AdaptToConsumer.hlsl") ||
			FAILED(device->CreateComputeShader(
				blob->GetBufferPointer(), blob->GetBufferSize(), nullptr,
				shader.put()))) {
			Logger::Get().Error("Create Frame Guidance consumer adapter shader failed");
			return false;
		}
		const D3D11_BUFFER_DESC bufferDesc{
			.ByteWidth = 32,
			.Usage = D3D11_USAGE_DYNAMIC,
			.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
			.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
		};
		if (FAILED(device->CreateBuffer(&bufferDesc, nullptr, params.put())) ||
			FAILED(device->CreateFence(
				0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())))) {
			Logger::Get().Error("Create Frame Guidance consumer adapter resources failed");
			return false;
		}
		return true;
	}

	void Reset() noexcept {
		entries.clear();
	}

	Entry* GetOrCreate(FrameGuidanceExtent extent) noexcept {
		for (const auto& entry : entries) {
			if (entry->extent == extent) return entry.get();
		}
		auto entry = std::make_unique<Entry>();
		entry->extent = extent;
		constexpr std::array<DXGI_FORMAT, 3> FORMATS{
			DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT_R32_FLOAT,
			DXGI_FORMAT_R8_UNORM
		};
		constexpr UINT BIND_FLAGS =
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		constexpr UINT MISC_FLAGS =
			D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
		for (size_t i = 0; i < FORMATS.size(); ++i) {
			entry->adaptedTextures[i] = DirectXHelper::CreateTexture2D(
				device, FORMATS[i], extent.width, extent.height, BIND_FLAGS,
				D3D11_USAGE_DEFAULT, MISC_FLAGS);
			entry->zeroTextures[i] = DirectXHelper::CreateTexture2D(
				device, FORMATS[i], extent.width, extent.height, BIND_FLAGS,
				D3D11_USAGE_DEFAULT, MISC_FLAGS);
			if (!entry->adaptedTextures[i] || !entry->zeroTextures[i] ||
				FAILED(device->CreateUnorderedAccessView(
					entry->adaptedTextures[i].get(), nullptr,
					entry->adaptedUavs[i].put())) ||
				FAILED(device->CreateUnorderedAccessView(
					entry->zeroTextures[i].get(), nullptr,
					entry->zeroUavs[i].put()))) {
				Logger::Get().Error(fmt::format(
					"Create Frame Guidance adapter textures failed for {}x{}",
					extent.width, extent.height));
				return nullptr;
			}
		}
		static constexpr float ZERO[4]{};
		for (const auto& uav : entry->zeroUavs) {
			context->ClearUnorderedAccessViewFloat(uav.get(), ZERO);
		}
		Entry* result = entry.get();
		entries.push_back(std::move(entry));
		Logger::Get().Info(fmt::format(
			"Frame Guidance consumer adapter created: target={}x{}",
			extent.width, extent.height));
		return result;
	}

	FrameGuidanceConsumerViews Adapt(
		const FrameGuidanceView& source,
		const FrameGuidanceView& sourceZero,
		FrameGuidanceFrameId frameId,
		FrameGuidanceExtent sourceExtent,
		FrameGuidanceExtent targetExtent
	) noexcept {
		Entry* entry = GetOrCreate(targetExtent);
		if (!entry) return {};
		if (entry->hasFrame && entry->frameId == frameId) {
			return { entry->produced, entry->zero, true, entry->fallbackActive };
		}

		const bool sourceValid = source.IsValidFor(frameId, sourceExtent);
		const bool sourceZeroValid = sourceZero.IsValidFor(frameId, sourceExtent);
		const bool allZero = sourceValid && source.depth.metadata.isZero &&
			source.motion.metadata.isZero && source.confidence.metadata.isZero;
		bool converted = sourceValid && sourceZeroValid;
		if (converted && !allZero) {
			const std::array<ID3D11Texture2D*, 3> sourceTextures{
				source.motion.texture, source.depth.texture, source.confidence.texture
			};
			for (size_t i = 0; i < sourceTextures.size(); ++i) {
				if (entry->sourceTextures[i] == sourceTextures[i] &&
					entry->sourceSrvs[i]) continue;
				entry->sourceSrvs[i] = nullptr;
				if (FAILED(device->CreateShaderResourceView(
					sourceTextures[i], nullptr, entry->sourceSrvs[i].put()))) {
					converted = false;
					break;
				}
				entry->sourceTextures[i] = sourceTextures[i];
			}
		}

		if (converted && !allZero) {
			struct Params {
				uint32_t sourceWidth, sourceHeight, targetWidth, targetHeight;
				float motionScaleX, motionScaleY, padding0, padding1;
			};
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (FAILED(context->Map(
				params.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
				converted = false;
			} else {
				const Params values{
					sourceExtent.width, sourceExtent.height,
					targetExtent.width, targetExtent.height,
					float(targetExtent.width) / float(sourceExtent.width),
					float(targetExtent.height) / float(sourceExtent.height), 0, 0
				};
				std::memcpy(mapped.pData, &values, sizeof(values));
				context->Unmap(params.get(), 0);
				ID3D11ShaderResourceView* srvs[]{
					entry->sourceSrvs[0].get(), entry->sourceSrvs[1].get(),
					entry->sourceSrvs[2].get()
				};
				ID3D11UnorderedAccessView* uavs[]{
					entry->adaptedUavs[0].get(), entry->adaptedUavs[1].get(),
					entry->adaptedUavs[2].get()
				};
				ID3D11Buffer* buffers[]{ params.get() };
				context->CSSetShader(shader.get(), nullptr, 0);
				context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
				context->CSSetUnorderedAccessViews(
					0, ARRAYSIZE(uavs), uavs, nullptr);
				context->CSSetConstantBuffers(0, 1, buffers);
				context->Dispatch(
					(targetExtent.width + 7) / 8,
					(targetExtent.height + 7) / 8, 1);
				ID3D11ShaderResourceView* nullSrvs[3]{};
				ID3D11UnorderedAccessView* nullUavs[3]{};
				context->CSSetShaderResources(0, ARRAYSIZE(nullSrvs), nullSrvs);
				context->CSSetUnorderedAccessViews(
					0, ARRAYSIZE(nullUavs), nullUavs, nullptr);
			}
		}

		const bool wasFallback = entry->fallbackActive;
		const uint64_t readyValue = ++fenceValue;
		if (FAILED(context->Signal(fence.get(), readyValue))) {
			converted = false;
		}
		entry->fallbackActive = !converted;
		const bool fallbackTransition = wasFallback != entry->fallbackActive;
		auto adaptZeroMetadata = [&](const FrameGuidanceMetadata& metadata) noexcept {
			FrameGuidanceMetadata result = sourceZeroValid ?
				AdaptMetadata(metadata, targetExtent, fence.get(), readyValue,
					fallbackTransition) :
				MakeTargetZeroMetadata(
					frameId, targetExtent, fence.get(), readyValue,
					fallbackTransition ? FrameGuidanceResetReason::ProviderFailure :
						FrameGuidanceResetReason::None,
					fallbackTransition);
			if (fallbackTransition) {
				result.resetReason = FrameGuidanceResetReason::ProviderFailure;
			}
			return result;
		};
		const FrameGuidanceMetadata zeroDepthMetadata =
			adaptZeroMetadata(sourceZero.depth.metadata);
		const FrameGuidanceMetadata zeroMotionMetadata =
			adaptZeroMetadata(sourceZero.motion.metadata);
		const FrameGuidanceMetadata zeroConfidenceMetadata =
			adaptZeroMetadata(sourceZero.confidence.metadata);
		entry->zero = {
			.depth = {
				entry->zeroTextures[1].get(), DXGI_FORMAT_R32_FLOAT,
				zeroDepthMetadata },
			.motion = {
				entry->zeroTextures[0].get(), DXGI_FORMAT_R16G16_FLOAT,
				zeroMotionMetadata },
			.confidence = {
				entry->zeroTextures[2].get(), DXGI_FORMAT_R8_UNORM,
				zeroConfidenceMetadata },
			.requiresHistoryReset =
				zeroDepthMetadata.requiresHistoryReset ||
				zeroMotionMetadata.requiresHistoryReset ||
				zeroConfidenceMetadata.requiresHistoryReset
		};
		if (!converted || allZero) {
			entry->produced = entry->zero;
		} else {
			const bool recovered = fallbackTransition;
			entry->produced = {
				.depth = {
					entry->adaptedTextures[1].get(), DXGI_FORMAT_R32_FLOAT,
					AdaptMetadata(source.depth.metadata, targetExtent,
						fence.get(), readyValue, recovered) },
				.motion = {
					entry->adaptedTextures[0].get(), DXGI_FORMAT_R16G16_FLOAT,
					AdaptMetadata(source.motion.metadata, targetExtent,
						fence.get(), readyValue, recovered) },
				.confidence = {
					entry->adaptedTextures[2].get(), DXGI_FORMAT_R8_UNORM,
					AdaptMetadata(source.confidence.metadata, targetExtent,
						fence.get(), readyValue, recovered) },
				.requiresHistoryReset = source.requiresHistoryReset || recovered
			};
		}
		entry->frameId = frameId;
		entry->hasFrame = true;
		if (!converted && fallbackTransition) {
			Logger::Get().Warn(fmt::format(
				"Frame Guidance adaptation failed at frameId={} ({}x{} -> {}x{}); "
				"using target-size Zero guidance",
				frameId, sourceExtent.width, sourceExtent.height,
				targetExtent.width, targetExtent.height));
		}
		return { entry->produced, entry->zero, true, !converted };
	}

	ID3D11Device5* device = nullptr;
	ID3D11DeviceContext4* context = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> shader;
	winrt::com_ptr<ID3D11Buffer> params;
	winrt::com_ptr<ID3D11Fence> fence;
	uint64_t fenceValue = 0;
	std::vector<std::unique_ptr<Entry>> entries;
};

static FrameGuidanceExtent GetTextureExtent(ID3D11Texture2D* texture) noexcept {
	if (!texture) {
		return {};
	}
	D3D11_TEXTURE2D_DESC desc{};
	texture->GetDesc(&desc);
	return { desc.Width, desc.Height };
}

static std::string_view ResetReasonName(
	FrameGuidanceResetReason reason
) noexcept {
	switch (reason) {
	case FrameGuidanceResetReason::None: return "None";
	case FrameGuidanceResetReason::Initialize: return "Initialize";
	case FrameGuidanceResetReason::Resize: return "Resize";
	case FrameGuidanceResetReason::SceneChange: return "SceneChange";
	case FrameGuidanceResetReason::CaptureInterrupted: return "CaptureInterrupted";
	case FrameGuidanceResetReason::DeviceRecreated: return "DeviceRecreated";
	case FrameGuidanceResetReason::LongPause: return "LongPause";
	case FrameGuidanceResetReason::ProviderFailure: return "ProviderFailure";
	default: return "Unknown";
	}
}

FrameGuidanceService::FrameGuidanceService() noexcept :
	_zeroDepthProvider(_zeroResources),
	_zeroMotionProvider(_zeroResources) {}

FrameGuidanceService::~FrameGuidanceService() = default;

bool FrameGuidanceService::SetDepthProvider(
	std::unique_ptr<IDepthProvider> provider
) noexcept {
	if (IsInitialized()) {
		return false;
	}
	_depthProvider = std::move(provider);
	return true;
}

bool FrameGuidanceService::SetMotionVectorProvider(
	std::unique_ptr<IMotionVectorProvider> provider
) noexcept {
	if (IsInitialized()) {
		return false;
	}
	_motionProvider = std::move(provider);
	return true;
}

bool FrameGuidanceService::Initialize(
	DeviceResources& resources,
	ID3D11Texture2D* sourceFrame,
	const FrameGuidanceRequirements& requirements
) noexcept {
	static_cast<void>(requirements);
	_resources = &resources;
	_sourceExtent = GetTextureExtent(sourceFrame);
	if (!_sourceExtent.IsValid() ||
		!_zeroDepthProvider.Initialize(resources, _sourceExtent) ||
		!_zeroMotionProvider.Initialize(resources, _sourceExtent)) {
		_resources = nullptr;
		Logger::Get().Error("Initialize Frame Guidance zero providers failed");
		return false;
	}
	_depthProviderReady = !_depthProvider ||
		_depthProvider->Initialize(resources, _sourceExtent);
	_motionProviderReady = !_motionProvider ||
		_motionProvider->Initialize(resources, _sourceExtent);
	if (!_depthProviderReady) {
		Logger::Get().Warn(
			"Frame Guidance depth provider initialization failed; using Zero Depth");
		_zeroDepthProvider.Reset(FrameGuidanceResetReason::ProviderFailure);
	}
	if (!_motionProviderReady) {
		Logger::Get().Warn(
			"Frame Guidance motion provider initialization failed; using Zero Motion");
		_zeroMotionProvider.Reset(FrameGuidanceResetReason::ProviderFailure);
	}

	// The frame-source output is only an allocation at this point; capture has
	// not started yet, so its contents are undefined. Keep every provider in its
	// Initialize reset state and let the first real capture frame seed history.
	_hasCachedFrame = false;
	_view = {};
	_zeroView = {};
	return true;
}

const FrameGuidanceView& FrameGuidanceService::BeginFrame(
	FrameGuidanceFrameId frameId,
	ID3D11Texture2D* sourceFrame,
	const FrameGuidanceRequirements& requirements
) noexcept {
	const FrameGuidanceExtent extent = GetTextureExtent(sourceFrame);
	if (_hasCachedFrame && _cachedFrameId == frameId && extent == _sourceExtent &&
		_cachedRequirements == requirements) {
		return _view;
	}
	if (extent != _sourceExtent) {
		if (!Resize(extent, requirements)) {
			return _view;
		}
	}
	return _Produce({
		.color = sourceFrame,
		.frameId = frameId,
		.sourceExtent = _sourceExtent,
		.validRegion = FrameGuidanceRegion::Full(_sourceExtent)
	}, requirements);
}

bool FrameGuidanceService::Resize(
	FrameGuidanceExtent sourceExtent,
	const FrameGuidanceRequirements& requirements
) noexcept {
	if (!sourceExtent.IsValid() || !_resources) {
		return false;
	}
	if (!_zeroDepthProvider.Resize(sourceExtent) ||
		!_zeroMotionProvider.Resize(sourceExtent)) {
		Logger::Get().Error("Resize Frame Guidance zero providers failed");
		return false;
	}
	if (requirements.depth && _depthProviderReady && _depthProvider &&
		!_depthProvider->Resize(sourceExtent)) {
		_depthProviderReady = false;
		_zeroDepthProvider.Reset(FrameGuidanceResetReason::ProviderFailure);
		Logger::Get().Warn(
			"Resize Frame Guidance depth provider failed; using Zero Depth");
	}
	if (requirements.motion && _motionProviderReady && _motionProvider &&
		!_motionProvider->Resize(sourceExtent)) {
		_motionProviderReady = false;
		_zeroMotionProvider.Reset(FrameGuidanceResetReason::ProviderFailure);
		Logger::Get().Warn(
			"Resize Frame Guidance motion provider failed; using Zero Motion");
	}
	_sourceExtent = sourceExtent;
	if (_adapterCache) _adapterCache->Reset();
	_hasCachedFrame = false;
	_view = {};
	_zeroView = {};
	return true;
}

FrameGuidanceConsumerViews FrameGuidanceService::GetConsumerViews(
	FrameGuidanceFrameId frameId,
	FrameGuidanceExtent targetExtent
) noexcept {
	if (!targetExtent.IsValid() || !_view.IsValidFor(frameId, _sourceExtent) ||
		!_zeroView.IsValidFor(frameId, _sourceExtent)) {
		return {};
	}
	if (targetExtent == _sourceExtent) {
		return { _view, _zeroView, false, false };
	}
	if (!_adapterCache) {
		_adapterCache = std::make_unique<AdapterCache>();
		if (!_resources || !_adapterCache->Initialize(*_resources)) {
			_adapterCache.reset();
			Logger::Get().Warn(
				"Frame Guidance consumer-size adaptation is unavailable");
			return {};
		}
	}
	return _adapterCache->Adapt(
		_view, _zeroView, frameId, _sourceExtent, targetExtent);
}

void FrameGuidanceService::ResetHistory(
	FrameGuidanceResetReason reason
) noexcept {
	Logger::Get().Info(fmt::format(
		"Frame Guidance history reset: reason={}", ResetReasonName(reason)));
	_zeroDepthProvider.Reset(reason);
	_zeroMotionProvider.Reset(reason);
	if (_depthProvider) {
		_depthProvider->Reset(reason);
	}
	if (_motionProvider) {
		_motionProvider->Reset(reason);
	}
	_view.requiresHistoryReset = true;
	_view.depth.metadata.requiresHistoryReset = true;
	_view.depth.metadata.resetReason = reason;
	_view.motion.metadata.requiresHistoryReset = true;
	_view.motion.metadata.resetReason = reason;
	_view.confidence.metadata.requiresHistoryReset = true;
	_view.confidence.metadata.resetReason = reason;
	_hasCachedFrame = false;
}

const FrameGuidanceView& FrameGuidanceService::_Produce(
	const FrameGuidanceFrame& frame,
	const FrameGuidanceRequirements& requirements
) noexcept {
	if (!_hasLoggedRequirements || requirements != _lastLoggedRequirements) {
		Logger::Get().Info(fmt::format(
			"Frame Guidance requirements frameId={}: zero={} motion={} depth={} "
			"depthInterval={} motionAction={} depthAction={}",
			frame.frameId, requirements.zero,
			requirements.motion, requirements.depth,
			requirements.depthInferenceInterval,
			requirements.motion ?
				(_motionProviderReady && _motionProvider ? "run" : "zero-unavailable") :
				"skip-unrequested",
			requirements.depth ?
				(_depthProviderReady && _depthProvider ? "run" : "zero-unavailable") :
				"skip-unrequested"));
		_lastLoggedRequirements = requirements;
		_hasLoggedRequirements = true;
	}
	DepthProviderOutput zeroDepth;
	MotionVectorProviderOutput zeroMotion;
	if (!_zeroDepthProvider.BeginFrame(frame, zeroDepth) ||
		!_zeroMotionProvider.BeginFrame(frame, zeroMotion) ||
		!zeroDepth.depth.IsValid(
			DXGI_FORMAT_R32_FLOAT, frame.frameId, frame.sourceExtent) ||
		!zeroMotion.motion.IsValid(
			DXGI_FORMAT_R16G16_FLOAT, frame.frameId, frame.sourceExtent) ||
		!zeroMotion.confidence.IsValid(
			DXGI_FORMAT_R8_UNORM, frame.frameId, frame.sourceExtent)) {
		Logger::Get().Error("Produce zero Frame Guidance failed");
		_view = {};
		_zeroView = {};
		_hasCachedFrame = false;
		return _view;
	}
	_zeroView = {
		.depth = zeroDepth.depth,
		.motion = zeroMotion.motion,
		.confidence = zeroMotion.confidence,
		.requiresHistoryReset =
			zeroDepth.depth.metadata.requiresHistoryReset ||
			zeroMotion.motion.metadata.requiresHistoryReset
	};

	MotionVectorProviderOutput motion;
	bool motionValid = false;
	const bool attemptedMotionProvider = requirements.motion &&
		_motionProviderReady && _motionProvider;
	if (attemptedMotionProvider) {
		motionValid = _motionProvider->BeginFrame(frame, motion) &&
			motion.motion.IsValid(
				DXGI_FORMAT_R16G16_FLOAT, frame.frameId, frame.sourceExtent) &&
			motion.confidence.IsValid(
				DXGI_FORMAT_R8_UNORM, frame.frameId, frame.sourceExtent) &&
			motion.motion.metadata.validRegion ==
				motion.confidence.metadata.validRegion;
	}
	if (!motionValid) {
		motion = zeroMotion;
		motionValid = true;
		if (attemptedMotionProvider) {
			motion.motion.metadata.resetReason =
				FrameGuidanceResetReason::ProviderFailure;
			motion.confidence.metadata.resetReason =
				FrameGuidanceResetReason::ProviderFailure;
			motion.motion.metadata.requiresHistoryReset = true;
			motion.confidence.metadata.requiresHistoryReset = true;
		}
	}

	FrameGuidanceFrame depthFrame = frame;
	depthFrame.motionGuidance = &motion;
	DepthProviderOutput depth;
	bool depthValid = false;
	const bool attemptedDepthProvider = requirements.depth &&
		_depthProviderReady && _depthProvider;
	if (attemptedDepthProvider) {
		depthValid = _depthProvider->BeginFrame(depthFrame, depth) &&
			depth.depth.IsValid(
				DXGI_FORMAT_R32_FLOAT, frame.frameId, frame.sourceExtent);
	}
	if (!depthValid) {
		depth = zeroDepth;
		depthValid = true;
		if (attemptedDepthProvider) {
			depth.depth.metadata.resetReason =
				FrameGuidanceResetReason::ProviderFailure;
			depth.depth.metadata.requiresHistoryReset = true;
		}
	}

	_view.depth = depth.depth;
	_view.motion = motion.motion;
	_view.confidence = motion.confidence;
	_view.rawDepth = depth.rawDepth;
	_view.depthResidual = depth.depthResidual;
	_view.requiresHistoryReset =
		_view.depth.metadata.requiresHistoryReset ||
		_view.motion.metadata.requiresHistoryReset ||
		_view.confidence.metadata.requiresHistoryReset;
	if (!_view.IsValidFor(frame.frameId, frame.sourceExtent)) {
		Logger::Get().Warn(fmt::format(
			"Frame Guidance coherence failure at frameId={}; using whole Zero group",
			frame.frameId));
		_view = _zeroView;
		_view.requiresHistoryReset = true;
	}
	_cachedFrameId = frame.frameId;
	_cachedRequirements = requirements;
	_hasCachedFrame = true;
	return _view;
}

}
