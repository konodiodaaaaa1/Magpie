#pragma once
#include "FrameGuidanceTypes.h"
#include "ScalingOptions.h"
#include "HdrEffectBoundary.h"
#include <utility>

namespace Magpie {

class DeviceResources;

struct NativeEffectDrawContext {
	ID3D11Texture2D* input = nullptr;
	ID3D11Texture2D* output = nullptr;
	HdrFrameMetadata inputMetadata{};
	HdrFrameMetadata outputMetadata{};
	FrameGuidanceFrameId frameId = 0;
	// Changes whenever an earlier effect in the chain changes its output for
	// the same captured frame. Native effects that cache duplicate frames must
	// include this value in their cache key.
	uint64_t inputRevision = 0;
	const FrameGuidanceView& frameGuidance;
	const FrameGuidanceView& zeroFrameGuidance;
};

// Common lifetime and rendering contract for native SDK-backed effects.
// Creation parameters remain in NativeEffectBackendFactory so Renderer does
// not need one parallel container and one name-dispatch branch per SDK.
class NativeEffectBackend {
public:
	virtual ~NativeEffectBackend() = default;

	virtual void SetHdrBoundary(HdrEffectBoundaryContext context) noexcept { _hdrBoundary = std::move(context); }
	const HdrEffectBoundaryContext& GetHdrBoundary() const noexcept { return _hdrBoundary; }

	virtual FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept {
		return {};
	}
	virtual bool Drain() noexcept { return true; }

	virtual EffectParameterApplyMode GetParameterApplyMode(
		std::string_view /*parameterName*/
	) const noexcept {
		return EffectParameterApplyMode::RestartRequired;
	}

	virtual EffectParameterRestartReason GetParameterRestartReason(
		std::string_view /*parameterName*/
	) const noexcept {
		return EffectParameterRestartReason::NativeBackend;
	}

	// Called on the backend thread with a complete candidate option. Implementations
	// must not recreate resources or partially commit settings when validation fails.
	virtual bool ApplyLiveParameters(
		const EffectOption& /*option*/,
		std::span<const std::string> /*parameterNames*/
	) noexcept {
		return false;
	}

	virtual bool Resize(
		DeviceResources& resources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output
	) noexcept = 0;

	virtual bool Draw(const NativeEffectDrawContext& context) noexcept = 0;

protected:
	HdrEffectBoundaryContext _hdrBoundary{};
};

}
