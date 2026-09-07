#pragma once
#include "FrameGuidanceTypes.h"

namespace Magpie {

class DeviceResources;
class NgxD3D12Core;

struct DLSSFrameGenerationSettings {
	uint32_t multiplier = 2;
	NvidiaOpticalFlowQuality motionVectorQuality =
		NvidiaOpticalFlowQuality::Balanced;
};

// Experimental DLSS Frame Generation adapter. It consumes final effect-chain
// color plus Renderer-owned guidance from the same captured base frame.
class DLSSFrameGenerator {
public:
	struct Impl;
	using PublishCallback = std::function<bool(ID3D11Texture2D*)>;

	DLSSFrameGenerator();
	DLSSFrameGenerator(const DLSSFrameGenerator&) = delete;
	DLSSFrameGenerator& operator=(const DLSSFrameGenerator&) = delete;
	~DLSSFrameGenerator();

	bool Initialize(
		DeviceResources& resources,
		NgxD3D12Core& ngxCore,
		ID3D11Texture2D* input,
		FrameGuidanceExtent guidanceExtent,
		const DLSSFrameGenerationSettings& settings
	) noexcept;
	bool Resize(
		DeviceResources& resources,
		NgxD3D12Core& ngxCore,
		ID3D11Texture2D* input,
		FrameGuidanceExtent guidanceExtent
	) noexcept;
	bool Draw(
		ID3D11Texture2D* input,
		FrameGuidanceFrameId frameId,
		const FrameGuidanceView& guidance,
		const FrameGuidanceView& zeroGuidance,
		const PublishCallback& publishGeneratedFrame) noexcept;
	void RequestHistoryReset() noexcept;
	bool Drain() noexcept;

	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept;
	const DLSSFrameGenerationSettings& Settings() const noexcept {
		return _requestedSettings;
	}
	uint32_t Multiplier() const noexcept;
	uint32_t MaxSupportedMultiplier() const noexcept;

private:
	std::unique_ptr<Impl> _impl;
	DLSSFrameGenerationSettings _requestedSettings{};
};

}
