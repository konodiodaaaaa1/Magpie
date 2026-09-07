#include "pch.h"
#include "FrameGuidanceD3D12Interop.h"
#include "Logger.h"

namespace Magpie {

bool FrameGuidanceD3D12Interop::Initialize(
	ID3D12Device* device,
	ID3D12Fence* consumerFence
) noexcept {
	_device = device;
	_consumerFence = consumerFence;
	return _device && _consumerFence;
}

bool FrameGuidanceD3D12Interop::_WaitForConsumer() noexcept {
	if (!_lastConsumerFenceValue ||
		_consumerFence->GetCompletedValue() >= _lastConsumerFenceValue) {
		return true;
	}
	wil::unique_event_nothrow event;
	if (FAILED(event.create()) || FAILED(_consumerFence->SetEventOnCompletion(
		_lastConsumerFenceValue, event.get()))) {
		return false;
	}
	event.wait();
	return true;
}

bool FrameGuidanceD3D12Interop::_OpenShared(
	ID3D11Texture2D* texture11,
	winrt::com_ptr<ID3D12Resource>& texture12
) noexcept {
	winrt::com_ptr<IDXGIResource1> dxgiResource;
	HRESULT hr = texture11->QueryInterface(IID_PPV_ARGS(dxgiResource.put()));
	if (FAILED(hr)) return false;
	HANDLE rawHandle = nullptr;
	hr = dxgiResource->CreateSharedHandle(
		nullptr, GENERIC_ALL, nullptr, &rawHandle);
	if (FAILED(hr)) return false;
	wil::unique_handle handle(rawHandle);
	return SUCCEEDED(_device->OpenSharedHandle(
		handle.get(), IID_PPV_ARGS(texture12.put())));
}

bool FrameGuidanceD3D12Interop::Update(
	const FrameGuidanceView& view,
	FrameGuidanceFrameId frameId,
	FrameGuidanceExtent extent
) noexcept {
	if (!view.IsValidFor(frameId, extent) ||
		view.motionDirection != FrameGuidanceMotionDirection::CurrentToPrevious ||
		view.motionUnit != FrameGuidanceMotionUnit::SourcePixels) {
		return false;
	}
	if (_motion11 == view.motion.texture && _depth11 == view.depth.texture &&
		_motion12 && _depth12) {
		return true;
	}
	if (!_WaitForConsumer()) return false;
	winrt::com_ptr<ID3D12Resource> motion;
	winrt::com_ptr<ID3D12Resource> depth;
	if (!_OpenShared(view.motion.texture, motion) ||
		!_OpenShared(view.depth.texture, depth)) {
		Logger::Get().Warn("Open Frame Guidance D3D11/D3D12 resources failed");
		return false;
	}
	_motion11 = view.motion.texture;
	_depth11 = view.depth.texture;
	_motion12 = std::move(motion);
	_depth12 = std::move(depth);
	return true;
}

bool FrameGuidanceD3D12Interop::WaitForProducer(
	ID3D11DeviceContext4* context,
	const FrameGuidanceView& view
) noexcept {
	for (const FrameGuidanceResource* resource : { &view.motion, &view.depth }) {
		if (resource->metadata.sync.fence && resource->metadata.sync.value &&
			FAILED(context->Wait(
				resource->metadata.sync.fence, resource->metadata.sync.value))) {
			return false;
		}
	}
	return true;
}

void FrameGuidanceD3D12Interop::Transition(
	ID3D12GraphicsCommandList* commandList,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after
) noexcept {
	D3D12_RESOURCE_BARRIER barriers[2]{};
	for (uint32_t i = 0; i < ARRAYSIZE(barriers); ++i) {
		barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[i].Transition = {
			i == 0 ? _motion12.get() : _depth12.get(),
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after
		};
	}
	commandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
}

}
