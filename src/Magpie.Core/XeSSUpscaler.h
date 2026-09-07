#pragma once
#include "NativeEffectBackend.h"

namespace Magpie {

class DeviceResources;

// Experimental colour-only XeSS-SR adapter. Magpie renders with D3D11, while
// the cross-vendor XeSS path is D3D12, so resources are shared between APIs.
class XeSSUpscaler final : public NativeEffectBackend {
public:
	struct Impl;

	XeSSUpscaler();
	XeSSUpscaler(const XeSSUpscaler&) = delete;
	XeSSUpscaler& operator=(const XeSSUpscaler&) = delete;
	~XeSSUpscaler() override;

	bool Initialize(
		DeviceResources& deviceResources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		MotionVectorRequest motionRequest = {}
	) noexcept;

	bool Resize(
		DeviceResources& deviceResources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output
	) noexcept override;

	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept override {
		FrameGuidanceRequirements result{ .zero = true };
		result.Add(_motionRequest);
		return result;
	}

	EffectParameterRestartReason GetParameterRestartReason(std::string_view) const noexcept override {
		return EffectParameterRestartReason::FrameGuidance;
	}

private:
	MotionVectorRequest _motionRequest{};
	std::unique_ptr<Impl> _impl;
};

}
