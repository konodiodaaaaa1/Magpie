#pragma once
#include "FrameGuidanceProvider.h"
#include "GroupBEffectProtocol.h"

namespace Magpie {

// Cross-vendor FidelityFX Optical Flow provider. "AMD OF" identifies the
// algorithm, not a vendor restriction: any D3D12 adapter satisfying the
// shader-model, wave-op and format requirements may create it.
class AmdOpticalFlowProvider final : public IMotionVectorProvider {
public:
	struct Impl;

	explicit AmdOpticalFlowProvider(
		AmdOpticalFlowMode mode = AmdOpticalFlowMode::Quality);
	AmdOpticalFlowProvider(const AmdOpticalFlowProvider&) = delete;
	AmdOpticalFlowProvider& operator=(const AmdOpticalFlowProvider&) = delete;
	~AmdOpticalFlowProvider() override;

	bool Initialize(
		DeviceResources& resources,
		FrameGuidanceExtent sourceExtent
	) noexcept override;
	bool BeginFrame(
		const FrameGuidanceFrame& frame,
		MotionVectorProviderOutput& output
	) noexcept override;
	void Reset(FrameGuidanceResetReason reason) noexcept override;
	bool Resize(FrameGuidanceExtent sourceExtent) noexcept override;
	OpticalFlowInitializationError InitializationError() const noexcept override;
	void SetHdrProtocol(const AmdOpticalFlowHdrProtocol& protocol) noexcept { _hdrProtocol = protocol; }

private:
	AmdOpticalFlowMode _mode;
	std::unique_ptr<Impl> _impl;
	AmdOpticalFlowHdrProtocol _hdrProtocol{};
};

}
