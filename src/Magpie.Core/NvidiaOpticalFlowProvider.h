#pragma once
#include "FrameGuidanceProvider.h"

namespace Magpie {

// One Renderer-owned D3D11 NVOF session. The NVIDIA driver DLL is resolved at
// runtime; the application never links or redistributes nvofapi64.dll.
class NvidiaOpticalFlowProvider final : public IMotionVectorProvider {
public:
	struct Impl;

	explicit NvidiaOpticalFlowProvider(
		NvidiaOpticalFlowQuality quality = NvidiaOpticalFlowQuality::Balanced);
	NvidiaOpticalFlowProvider(const NvidiaOpticalFlowProvider&) = delete;
	NvidiaOpticalFlowProvider& operator=(
		const NvidiaOpticalFlowProvider&) = delete;
	~NvidiaOpticalFlowProvider() override;

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

private:
	NvidiaOpticalFlowQuality _quality;
	std::unique_ptr<Impl> _impl;
};

}
