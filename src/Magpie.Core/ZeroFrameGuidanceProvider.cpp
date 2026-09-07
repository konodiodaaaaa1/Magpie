#include "pch.h"
#include "ZeroFrameGuidanceProvider.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"
#include "ScalingWindow.h"

namespace Magpie {

bool ZeroFrameGuidanceResources::Initialize(
	DeviceResources& resources,
	FrameGuidanceExtent sourceExtent
) noexcept {
	_device = resources.GetD3DDevice();
	_context = resources.GetD3DDC();
	return _CreateTextures(sourceExtent);
}

bool ZeroFrameGuidanceResources::Resize(FrameGuidanceExtent sourceExtent) noexcept {
	return _CreateTextures(sourceExtent);
}

void ZeroFrameGuidanceResources::Reset() noexcept {
	_depth = nullptr;
	_motion = nullptr;
	_confidence = nullptr;
	_extent = {};
	_context = nullptr;
	_device = nullptr;
}

bool ZeroFrameGuidanceResources::_CreateTextures(
	FrameGuidanceExtent sourceExtent
) noexcept {
	if (!_device || !_context || !sourceExtent.IsValid()) {
		return false;
	}
	if (_extent == sourceExtent && _depth && _motion && _confidence) {
		return true;
	}

	constexpr UINT BIND_FLAGS =
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	auto depth = DirectXHelper::CreateSharedTexture2D(
		_device, DXGI_FORMAT_R32_FLOAT, sourceExtent.width, sourceExtent.height,
		BIND_FLAGS, "Zero/Depth");
	auto motion = DirectXHelper::CreateSharedTexture2D(
		_device, DXGI_FORMAT_R16G16_FLOAT, sourceExtent.width, sourceExtent.height,
		BIND_FLAGS, "Zero/Motion");
	auto confidence = DirectXHelper::CreateSharedTexture2D(
		_device, DXGI_FORMAT_R8_UNORM, sourceExtent.width, sourceExtent.height,
		BIND_FLAGS, "Zero/Confidence");
	if (!depth || !motion || !confidence) {
		Logger::Get().Error("Create zero Frame Guidance textures failed");
		return false;
	}

	winrt::com_ptr<ID3D11UnorderedAccessView> depthUav;
	winrt::com_ptr<ID3D11UnorderedAccessView> motionUav;
	winrt::com_ptr<ID3D11UnorderedAccessView> confidenceUav;
	HRESULT hr = _device->CreateUnorderedAccessView(
		depth.get(), nullptr, depthUav.put());
	if (SUCCEEDED(hr)) {
		hr = _device->CreateUnorderedAccessView(
			motion.get(), nullptr, motionUav.put());
	}
	if (SUCCEEDED(hr)) {
		hr = _device->CreateUnorderedAccessView(
			confidence.get(), nullptr, confidenceUav.put());
	}
	if (FAILED(hr)) {
		Logger::Get().ComError("Create zero Frame Guidance UAVs failed", hr);
		return false;
	}

	static constexpr float ZERO[4]{};
	static constexpr float ONE[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	const float* depthClear = ScalingWindow::Get().Options().IsHdrCompatibilityEnabled()
		? ONE : ZERO;
	_context->ClearUnorderedAccessViewFloat(depthUav.get(), depthClear);
	_context->ClearUnorderedAccessViewFloat(motionUav.get(), ZERO);
	_context->ClearUnorderedAccessViewFloat(confidenceUav.get(), ZERO);
	_depth = std::move(depth);
	_motion = std::move(motion);
	_confidence = std::move(confidence);
	_extent = sourceExtent;
	return true;
}

static FrameGuidanceMetadata MakeZeroMetadata(
	const FrameGuidanceFrame& frame,
	FrameGuidanceResetReason resetReason
) noexcept {
	return {
		.frameId = frame.frameId,
		.captureSequence = frame.captureSequence,
		.resourceGeneration = frame.resourceGeneration,
		.timestamp100ns = frame.timestamp100ns,
		.sourceExtent = frame.sourceExtent,
		.validRegion = frame.validRegion,
		.resetReason = resetReason,
		.valid = true,
		.isZero = true,
		.requiresHistoryReset = resetReason != FrameGuidanceResetReason::None
	};
}

bool ZeroDepthProvider::Initialize(
	DeviceResources& resources,
	FrameGuidanceExtent sourceExtent
) noexcept {
	_resetReason = FrameGuidanceResetReason::Initialize;
	return _resources->Initialize(resources, sourceExtent);
}

bool ZeroDepthProvider::BeginFrame(
	const FrameGuidanceFrame& frame,
	DepthProviderOutput& output
) noexcept {
	if (_resources->Extent() != frame.sourceExtent || !_resources->Depth()) {
		return false;
	}
	output.depth = {
		.texture = _resources->Depth(),
		.format = DXGI_FORMAT_R32_FLOAT,
		.metadata = MakeZeroMetadata(frame, _resetReason)
	};
	_resetReason = FrameGuidanceResetReason::None;
	return true;
}

void ZeroDepthProvider::Reset(FrameGuidanceResetReason reason) noexcept {
	_resetReason = reason;
}

bool ZeroDepthProvider::Resize(FrameGuidanceExtent sourceExtent) noexcept {
	_resetReason = FrameGuidanceResetReason::Resize;
	return _resources->Resize(sourceExtent);
}

bool ZeroMotionVectorProvider::Initialize(
	DeviceResources& resources,
	FrameGuidanceExtent sourceExtent
) noexcept {
	_resetReason = FrameGuidanceResetReason::Initialize;
	return _resources->Initialize(resources, sourceExtent);
}

bool ZeroMotionVectorProvider::BeginFrame(
	const FrameGuidanceFrame& frame,
	MotionVectorProviderOutput& output
) noexcept {
	if (_resources->Extent() != frame.sourceExtent || !_resources->Motion() ||
		!_resources->Confidence()) {
		return false;
	}
	const FrameGuidanceMetadata metadata = MakeZeroMetadata(frame, _resetReason);
	output.motion = {
		.texture = _resources->Motion(),
		.format = DXGI_FORMAT_R16G16_FLOAT,
		.metadata = metadata
	};
	output.confidence = {
		.texture = _resources->Confidence(),
		.format = DXGI_FORMAT_R8_UNORM,
		.metadata = metadata
	};
	_resetReason = FrameGuidanceResetReason::None;
	return true;
}

void ZeroMotionVectorProvider::Reset(FrameGuidanceResetReason reason) noexcept {
	_resetReason = reason;
}

bool ZeroMotionVectorProvider::Resize(FrameGuidanceExtent sourceExtent) noexcept {
	_resetReason = FrameGuidanceResetReason::Resize;
	return _resources->Resize(sourceExtent);
}

}
