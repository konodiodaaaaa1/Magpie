#pragma once
#include "FrameGuidanceTypes.h"

namespace Magpie {

class DeviceResources;

class PresenterBase {
public:
	virtual ~PresenterBase() noexcept {}

	bool Initialize(HWND hwndAttach, const DeviceResources& deviceResources) noexcept;

	virtual bool BeginFrame(
		winrt::com_ptr<ID3D11Texture2D>& frameTex,
		winrt::com_ptr<ID3D11RenderTargetView>& frameRtv,
		POINT& drawOffset
	) noexcept = 0;

	// Returns true when the frame was submitted to the presentation backend.
	// Successful status codes such as DXGI_STATUS_OCCLUDED still count as a
	// submission so a deliberately hidden first-frame window can be shown.
	virtual bool EndFrame(bool waitForGpu = false) noexcept = 0;
	// Only stable DXGI images may remain prepared across message-pump passes.
	virtual bool SupportsDeferredPresent() const noexcept { return false; }
	// SDK-owned frame generators implement their own input limiter.
	virtual bool SetBaseFrameRateLimit(double) noexcept { return true; }
	// Called only from the outer message pump, after BeginFrame found no capacity.
	virtual bool WaitForFrameCapacity(DWORD) noexcept { return false; }
	std::chrono::steady_clock::time_point LastSubmissionTime() const noexcept { return _lastSubmissionTime; }
	std::optional<uint32_t> LastPresentedFrameCount() const noexcept { return _lastPresentedFrameCount; }

	// Supplies motion that belongs to the next real base frame. Presenters that
	// do not consume frame guidance intentionally ignore this call.
	virtual void SetFrameGuidance(
		ID3D11Texture2D*,
		FrameGuidanceFrameId,
		bool,
		const RECT&
	) noexcept {}

	// Frame-generation presenters may expose a transparent composition surface
	// above their SDK-owned colour swap chain. UI-only presents use this surface
	// and therefore never masquerade as a new game input frame.
	virtual bool HasIndependentOverlay() const noexcept { return false; }
	virtual bool BeginOverlayFrame(
		winrt::com_ptr<ID3D11Texture2D>&,
		winrt::com_ptr<ID3D11RenderTargetView>&,
		POINT&
	) noexcept { return false; }
	virtual bool EndOverlayFrame() noexcept { return false; }

	// A presenter with a frame-latency waitable object already provides queue
	// capacity pacing in BeginFrame. DLSSFG must not add a DWM wait on top of it.
	virtual bool UsesFrameLatencyWaitableObject() const noexcept {
		return false;
	}

	virtual bool OnResize() noexcept = 0;

	virtual void OnEndResize(bool& shouldRedraw) noexcept {
		shouldRedraw = false;
	}

protected:
	virtual bool _Initialize(HWND hwndAttach) noexcept = 0;

	void _WaitForGpu() noexcept;

	static uint32_t _CalcBufferCount() noexcept;

	const DeviceResources* _deviceResources = nullptr;
	std::optional<uint32_t> _lastPresentedFrameCount = 0;
	std::chrono::steady_clock::time_point _lastSubmissionTime{};

private:
	winrt::com_ptr<ID3D11Fence> _fence;
	uint64_t _fenceValue = 0;
	wil::unique_event_nothrow _fenceEvent;
};

}
