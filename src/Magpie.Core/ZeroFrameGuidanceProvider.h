#pragma once
#include "FrameGuidanceProvider.h"

namespace Magpie {

class DeviceResources;

// The two zero providers share this texture set so all consumers observe one
// Renderer-owned fallback rather than allocating private guide textures.
class ZeroFrameGuidanceResources {
public:
	bool Initialize(
		DeviceResources& resources,
		FrameGuidanceExtent sourceExtent
	) noexcept;
	bool Resize(FrameGuidanceExtent sourceExtent) noexcept;
	void Reset() noexcept;

	ID3D11Texture2D* Depth() const noexcept { return _depth.get(); }
	ID3D11Texture2D* Motion() const noexcept { return _motion.get(); }
	ID3D11Texture2D* Confidence() const noexcept { return _confidence.get(); }
	FrameGuidanceExtent Extent() const noexcept { return _extent; }

private:
	bool _CreateTextures(FrameGuidanceExtent sourceExtent) noexcept;

	ID3D11Device5* _device = nullptr;
	ID3D11DeviceContext4* _context = nullptr;
	FrameGuidanceExtent _extent{};
	winrt::com_ptr<ID3D11Texture2D> _depth;
	winrt::com_ptr<ID3D11Texture2D> _motion;
	winrt::com_ptr<ID3D11Texture2D> _confidence;
};

class ZeroDepthProvider final {
public:
	explicit ZeroDepthProvider(ZeroFrameGuidanceResources& resources) noexcept :
		_resources(&resources) {}

	bool Initialize(DeviceResources& resources, FrameGuidanceExtent sourceExtent) noexcept;
	bool BeginFrame(const FrameGuidanceFrame& frame, DepthProviderOutput& output) noexcept;
	void Reset(FrameGuidanceResetReason reason) noexcept;
	bool Resize(FrameGuidanceExtent sourceExtent) noexcept;

private:
	ZeroFrameGuidanceResources* _resources = nullptr;
	FrameGuidanceResetReason _resetReason = FrameGuidanceResetReason::Initialize;
};

class ZeroMotionVectorProvider final : public IMotionVectorProvider {
public:
	explicit ZeroMotionVectorProvider(ZeroFrameGuidanceResources& resources) noexcept :
		_resources(&resources) {}

	bool Initialize(DeviceResources& resources, FrameGuidanceExtent sourceExtent) noexcept override;
	bool BeginFrame(const FrameGuidanceFrame& frame, MotionVectorProviderOutput& output) noexcept override;
	void Reset(FrameGuidanceResetReason reason) noexcept override;
	bool Resize(FrameGuidanceExtent sourceExtent) noexcept override;

private:
	ZeroFrameGuidanceResources* _resources = nullptr;
	FrameGuidanceResetReason _resetReason = FrameGuidanceResetReason::Initialize;
};

}
