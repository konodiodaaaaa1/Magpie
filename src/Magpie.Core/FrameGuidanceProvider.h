#pragma once
#include "FrameGuidanceTypes.h"

namespace Magpie {

class DeviceResources;

class IMotionVectorProvider {
public:
	virtual ~IMotionVectorProvider() = default;

	virtual bool Initialize(
		DeviceResources& resources,
		FrameGuidanceExtent sourceExtent
	) noexcept = 0;
	virtual bool BeginFrame(
		const FrameGuidanceFrame& frame,
		MotionVectorProviderOutput& output
	) noexcept = 0;
	virtual void Reset(FrameGuidanceResetReason reason) noexcept = 0;
	virtual bool Resize(FrameGuidanceExtent sourceExtent) noexcept = 0;
	virtual OpticalFlowInitializationError InitializationError() const noexcept {
		return OpticalFlowInitializationError::ProviderUnavailable;
	}
};

}
