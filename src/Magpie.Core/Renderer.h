#pragma once
#include "BackendDescriptorStore.h"
#include "CursorDrawer.h"
#include "DeviceResources.h"
#include "EffectDrawer.h"
#include "EffectsProfiler.h"
#include "FrameGuidanceService.h"
#include "NgxD3D12Core.h"
#include "OverlayDrawer.h"
#include "PassThroughFrames.h"
#include "PresenterBase.h"
#include "PresentationFrameRate.h"
#include "FramePresentationTiming.h"
#include "ScalingOptions.h"
#include "StepTimer.h"
#include <mutex>
#include <unordered_map>

namespace Magpie {

class FrameSourceBase;

enum class DLSSFGFrameRenderResult : uint8_t {
	Presented,
	Retry,
	Dropped
};

class Renderer {
public:
	Renderer() noexcept;
	~Renderer() noexcept;

	Renderer(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;

	ScalingError Initialize(HWND hwndAttach, OverlayOptions& overlayOptions) noexcept;
	const std::string& InitializationContext() const noexcept { return _backendInitContext; }
	const std::wstring& MotionConfigurationNotice() const noexcept { return _motionConfigurationNotice; }

	bool Render(bool force = false, bool waitForGpu = false) noexcept;
	bool RenderOverlay() noexcept;
	bool HasFrameGeneration() const noexcept { return _hasFrameGeneration; }
	PresentationRateSnapshot PresentationRate() const noexcept { return _presentationRate.Get(); }
	DLSSFGFrameRenderResult RenderDLSSFGFrame(
		uint32_t sharedTextureSlot,
		uint32_t sharedTextureGeneration
	) noexcept;
	bool HasPendingOverlayInput() const noexcept;
	bool HasUrgentOverlayInput() const noexcept;
	bool HasPendingContent() const noexcept;
	std::chrono::nanoseconds FrontendPollInterval() const noexcept {
		return _overlayPresentationClock.PollInterval();
	}

	// Sleep in the outer message pump, never inside a prepared frame.
	void WaitForFrontendWork(std::chrono::nanoseconds maximumWait) noexcept;
	bool OnResize() noexcept;

	void OnEndResize() noexcept;

	void OnMove() noexcept;

	void SwitchToolbarState() noexcept;
	void InvokeOverlayAction(OverlayAction action) noexcept;
	bool IsPassThroughActive() const noexcept { return _isPassThroughActive; }
	bool SetPassThroughActive(bool value) noexcept;
	void TakeDisplayedScreenshot() noexcept {
		TakeScreenshot(std::numeric_limits<uint32_t>::max());
	}

	const RECT& SrcRect() const noexcept;

	// 屏幕坐标而不是窗口局部坐标
	const RECT& DestRect() const noexcept {
		return _destRect;
	}

	const FrameSourceBase& FrameSource() const noexcept {
		return *_frameSource;
	}

	void OnCursorVisibilityChanged(bool isVisible, bool onDestory);

	void OnSourceFocusChanged() noexcept;

	bool MessageHandler(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
	void ClearOverlayStates() noexcept;
	bool IsEffectParameterInputActive() const noexcept { return _overlayDrawer.IsEffectParameterInputActive(); }
	bool IsEffectParametersVisible() const noexcept { return _overlayDrawer.IsEffectParametersVisible(); }

	const std::vector<const EffectDesc*>& ActiveEffectDescs() const noexcept {
		return _activeEffectDescs;
	}

	const std::vector<std::vector<EffectParameterRuntimeInfo>>&
		EffectParameterRuntimeInfos() const noexcept {
		return _effectParameterRuntimeInfos;
	}

	bool QueueEffectParameterUpdate(
		uint32_t effectIdx,
		uint32_t parameterIdx,
		float value,
		bool waitForOverlaySave = true
	) noexcept;

	void StartProfile() noexcept;

	void StopProfile() noexcept;

	bool IsCursorOnOverlayCaptionArea() const noexcept {
		return _overlayDrawer.IsCursorOnCaptionArea();
	}

	winrt::fire_and_forget TakeScreenshot(
		uint32_t effectIdx,
		uint32_t passIdx = std::numeric_limits<uint32_t>::max(),
		uint32_t outputIdx = std::numeric_limits<uint32_t>::max()
	) noexcept;

private:
	bool _frameTraceStarted = false;
	// Set before starting the backend; immutable for this session.
	bool _frontEdgeSyncEnabled = false;
	bool _frontEdgeUsesSharedSlot = false;
	bool _frontEdgeLimiterFailed = false;
	uint32_t _configuredFrameGenerationMultiplier = 1;
	std::atomic<double> _presentationRefreshRate = 60.0;
	std::atomic<double> _existingBaseFrameRateLimit = 0.0;
	double _FrontEdgeFrameRate() const noexcept;
	FrontEdgeSyncClock _frontEdgeClock;
	std::optional<std::chrono::steady_clock::time_point> _frontendPacingDeadline;
	wil::unique_handle _frontendPacingTimer;
	std::atomic<uint64_t> _frontEdgeAcknowledgedKey = 0;
	wil::unique_handle _frontEdgeConsumedEvent;
	// Backend-owned staged input retains NR/SR and guidance until FG is due.
	FrontEdgeSyncClock _fgInputClock;
	wil::unique_handle _fgInputTimer;
	winrt::com_ptr<ID3D11Texture2D> _pendingFrameGenerationInput;
	bool _backendMayDeferFG = false;
	void _CompleteBackendFrame(ID3D11Texture2D* effectsOutput, bool isNewCaptureFrame,
		uint64_t captureSequence) noexcept;
	void _LogHdrTextureStats(ID3D11Texture2D* texture, std::string_view label) noexcept;
	struct PendingFrontendFrame {
		bool stableBaseOnly = false;
		bool contentFrame = false;
		bool generatedFrame = false;
		bool independentOverlay = false;
		bool waitForGpu = false;
		bool paced = false;
		uint64_t overlayRevision = 0;
		uint64_t contentKey = 0;
	};
	std::optional<PendingFrontendFrame> _pendingFrontendFrame;
	bool _SubmitFrontendFrame() noexcept;
	OverlayPresentationClock _overlayPresentationClock;
	HMONITOR _overlayMonitor = nullptr;
	uint64_t _overlayActionRevision = 0;
	uint64_t _presentedOverlayActionRevision = 0;
	bool _HasPendingOverlayAction() const noexcept {
		return _overlayActionRevision != _presentedOverlayActionRevision;
	}
	void _UpdateOverlayRefreshRate() noexcept;
	bool _CanRenderOverlay() const noexcept;
	struct FrontendRenderTimings {
		std::chrono::nanoseconds beginFrame{};
		std::chrono::nanoseconds draw{};
		std::chrono::nanoseconds endFrame{};
	};

	bool _FrontendRender(
		bool waitForGpu = false,
		uint32_t sharedTextureSlot = std::numeric_limits<uint32_t>::max(),
		FrontendRenderTimings* timings = nullptr,
		bool stableBaseOnly = false
	) noexcept;
	bool _FrontendOverlayRender(bool contentChanged = false) noexcept;
	bool _UpdateFrontendBase(uint32_t sharedTextureSlot) noexcept;
	bool _OpenFrontendSharedTextures() noexcept;
	void _ResetDLSSFGSlotEvents() noexcept;
	void _CopySceneToTarget(ID3D11Texture2D* scene, ID3D11Texture2D* target,
		ID3D11RenderTargetView* rtv, POINT drawOffset) noexcept;
	void _RecordDLSSFGFrontendTimings(
		bool usesFrameLatencyWaitableObject,
		std::chrono::nanoseconds pacingWait,
		const FrontendRenderTimings& timings
	) noexcept;

	void _BackendThreadProc() noexcept;

	HANDLE _InitBackend() noexcept;

	bool _InitFrameSource() noexcept;

	ID3D11Texture2D* _BuildEffects() noexcept;

	void _UpdateHdrEffectBoundaryContexts() noexcept;

	void _UpdateActiveEffectDescs() noexcept;

	bool _ShouldAppendBicubic(ID3D11Texture2D* outTexture) noexcept;

	bool _AppendBicubic(ID3D11Texture2D** inOutTexture) noexcept;

	ID3D11Texture2D* _ResizeEffects() noexcept;

	void _BuildEffectParameterRuntimeInfos() noexcept;
	void _ApplyPendingEffectParameters() noexcept;
	void _UpdateFrameRateLimits() noexcept;

	void _UpdateDestRect() noexcept;

	HANDLE _CreateSharedTexture(ID3D11Texture2D* effectsOutput) noexcept;

	void _BackendRender(
		ID3D11Texture2D* effectsOutput,
		bool isNewCaptureFrame
	) noexcept;

	bool _PublishBackendTexture(
		ID3D11Texture2D* texture,
		bool synchronous,
		bool generatedFrame = false,
		uint64_t captureSequence = 0,
		uint64_t resourceGeneration = 0
	) noexcept;

	bool _InitializeDLSSFrameGenerator(
		ID3D11Texture2D* input,
		const struct DLSSFrameGenerationSettings& settings
	) noexcept;
	void _HandleDLSSFrameGenerationFailure(ID3D11Texture2D* input) noexcept;
	void _DisableDLSSFrameGenerationForSession() noexcept;
	bool _DrainNgxConsumers() noexcept;
	void _ReleaseNgxConsumers() noexcept;

	bool _UpdateDynamicConstants() const noexcept;


	winrt::IAsyncOperation<bool> _TakeScreenshotImpl(
		uint32_t effectIdx,
		uint32_t passIdx,
		uint32_t outputIdx
	) noexcept;

	static LRESULT CALLBACK _LowLevelKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam);

	// 只能由前台线程访问
	DeviceResources _frontendResources;
	std::unique_ptr<PresenterBase> _presenter;
	
	CursorDrawer _cursorDrawer;
	OverlayDrawer _overlayDrawer;
	PassThroughFrames _passThroughFrames;
	bool _isPassThroughActive = false;

	static constexpr uint32_t MAX_SHARED_TEXTURE_SLOTS = 4;
	std::array<winrt::com_ptr<ID3D11Texture2D>, MAX_SHARED_TEXTURE_SLOTS>
		_frontendSharedTextures;
	std::array<winrt::com_ptr<IDXGIKeyedMutex>, MAX_SHARED_TEXTURE_SLOTS>
		_frontendSharedTextureMutexes;
	std::array<winrt::com_ptr<ID3D11Texture2D>, MAX_SHARED_TEXTURE_SLOTS>
		_frontendSharedMotionTextures;
	std::array<winrt::com_ptr<IDXGIKeyedMutex>, MAX_SHARED_TEXTURE_SLOTS>
		_frontendSharedMotionTextureMutexes;
	std::array<uint64_t, MAX_SHARED_TEXTURE_SLOTS> _lastAccessMutexKeys{};
	std::array<std::mutex, MAX_SHARED_TEXTURE_SLOTS> _sharedTextureAccessMutexes;
	winrt::com_ptr<ID3D11Texture2D> _frontendBaseTexture;
	winrt::com_ptr<ID3D11Texture2D> _frontendPresentedBaseTexture;
	winrt::com_ptr<ID3D11Texture2D> _frontendMotionTexture;
	FrameGuidanceFrameId _frontendMotionFrameId = 0;
	bool _frontendMotionValid = false;
	bool _frontendMotionReset = true;
	bool _frontendBaseValid = false;
	bool _frontendPresentedBaseValid = false;
	bool _frontendBaseNeedsPresent = false;
	RECT _destRect{};
	
	std::thread _backendThread;

	wil::unique_hhook _hKeyboardHook;
	
	// 只能由后台线程访问
	DeviceResources _backendResources;
	Magpie::BackendDescriptorStore _backendDescriptorStore;
	std::unique_ptr<FrameSourceBase> _frameSource;
	FrameGuidanceService _frameGuidanceService;
	bool _hasFrameGeneration = false;
	mutable PresentationFrameRate _presentationRate;
	FrameGuidanceFrameId _capturedFrameId = 0;
	std::chrono::steady_clock::time_point _lastCapturedFrameTime{};
	NgxD3D12Core _ngxD3D12Core;
	std::vector<EffectDrawer> _effectDrawers;
	std::vector<EffectOption> _runtimeEffectOptions;
	std::vector<uint64_t> _effectInputRevisions;
	struct PendingEffectParameterUpdate {
		uint32_t effectIdx = 0;
		uint32_t parameterIdx = 0;
		float value = 0.0f;
	};
	std::vector<PendingEffectParameterUpdate> _pendingEffectParameterUpdates;
	bool _forceNextRender = false;
	std::vector<std::unique_ptr<class NativeEffectBackend>> _nativeEffectBackends;
	std::unique_ptr<class DLSSFrameGenerator> _dlssFrameGenerator;
	uint32_t _dlssFgConsecutiveFailures = 0;
	uint32_t _dlssFgRecoveryAttempts = 0;

	StepTimer _stepTimer;
	EffectsProfiler _effectsProfiler;

	winrt::com_ptr<ID3D11Fence> _d3dFence;
	uint64_t _fenceValue = 0;
	wil::unique_event_nothrow _fenceEvent;

	std::array<winrt::com_ptr<ID3D11Texture2D>, MAX_SHARED_TEXTURE_SLOTS>
		_backendSharedTextures;
	HdrSurfaceAdapter _hdrPresentationAdapter;
	winrt::com_ptr<ID3D11Texture2D> _hdrPresentationTexture;
	winrt::com_ptr<ID3D11Texture2D> _dlssFgNormalizedInput;
	winrt::com_ptr<ID3D11Texture2D> _dlssFgCanonicalGenerated;
	float _dlssFgHdrNormalizationScale = 1.0f;
	std::array<winrt::com_ptr<IDXGIKeyedMutex>, MAX_SHARED_TEXTURE_SLOTS>
		_backendSharedTextureMutexes;
	std::array<winrt::com_ptr<ID3D11Texture2D>, MAX_SHARED_TEXTURE_SLOTS>
		_backendSharedMotionTextures;
	std::array<winrt::com_ptr<IDXGIKeyedMutex>, MAX_SHARED_TEXTURE_SLOTS>
		_backendSharedMotionTextureMutexes;
	std::array<HANDLE, MAX_SHARED_TEXTURE_SLOTS> _sharedTextureHandles{};
	std::array<wil::unique_handle, MAX_SHARED_TEXTURE_SLOTS>
		_sharedMotionTextureHandles;
	std::array<wil::unique_handle, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureAvailableEvents;
	uint32_t _sharedTextureSlotCount = 1;
	uint32_t _nextBackendSharedTextureSlot = 0;

	winrt::com_ptr<ID3D11Buffer> _dynamicCB;


	// 可由所有线程访问
	std::array<std::atomic<uint64_t>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureMutexKeys{};
	std::array<std::atomic<bool>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureContainsGeneratedFrame{};
	std::array<std::atomic<FrameGuidanceFrameId>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedMotionFrameIds{};
	std::array<std::atomic<bool>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedMotionValid{};
	std::array<std::atomic<bool>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedMotionReset{};
	std::array<std::atomic<uint64_t>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureCaptureSequences{};
	std::array<std::atomic<FrameGuidanceFrameId>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureFrameIds{};
	std::array<std::atomic<uint64_t>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureResourceGenerations{};
	std::array<std::atomic<int64_t>, MAX_SHARED_TEXTURE_SLOTS>
		_sharedTextureTimestamps{};
	std::array<HdrFrameMetadata, MAX_SHARED_TEXTURE_SLOTS> _sharedFrameMetadata{};
	HdrFrameMetadata _frontendFrameMetadata{};
	HdrFrameMetadata _frontendPresentedFrameMetadata{};
	std::atomic<uint64_t> _activeCaptureSequence = 0;
	std::atomic<uint64_t> _activeResourceGeneration = 0;
	std::atomic<uint32_t> _latestSharedTextureSlot = 0;
	std::atomic<uint32_t> _sharedTextureGeneration = 0;
	std::atomic<bool> _synchronousFramePresentationEnabled = false;
	std::atomic<uint32_t> _pendingDLSSFGFrontendFrames = 0;
	float _frameRateFilterTarget = 0.0f;
	std::optional<float> _captureMaxFrameRate;
	// Backend-owned cadence; publish an immutable interval with each ring slot.
	CaptureFrameCadence _captureCadence;
	uint64_t _captureSequence = 0;
	uint32_t _publicationTimingSamples = 0;
	double _publicationTransactionTotalMs = 0;
	double _publicationTransactionMaxMs = 0;
	double _publicationFenceTotalMs = 0;
	double _publicationFenceMaxMs = 0;
	// Backend-only; separate from the frontend's exchanged diagnostic counters.
	std::chrono::steady_clock::duration _captureCadenceQueueWait{};
	double _baseFrameRateLimit = 0;
	std::chrono::nanoseconds _synchronousPresentInterval{};
	std::array<std::atomic<int64_t>, MAX_SHARED_TEXTURE_SLOTS> _sharedPresentIntervalNs{};
	FramePresentationClock _presentationClock;
	FrameGuidanceFrameId _frontendCaptureFrameId = 0;
	FrameGuidanceFrameId _lastCountedRealFrameId = 0;
	uint32_t _dlssFgFrontendTimingFrames = 0;
	bool _dlssFgFrontendTimingModeInitialized = false;
	bool _dlssFgFrontendTimingUsesWaitableObject = false;
	std::chrono::nanoseconds _dlssFgFrontendPacingWait{};
	std::chrono::nanoseconds _dlssFgFrontendBeginFrame{};
	std::chrono::nanoseconds _dlssFgFrontendDraw{};
	std::chrono::nanoseconds _dlssFgFrontendEndFrame{};
	std::atomic<uint64_t> _dlssFgRingWaitNanoseconds = 0;
	std::atomic<uint64_t> _dlssFgRingWaitSamples = 0;
	std::chrono::steady_clock::time_point _dlssFgDiagnosticsStart{};
	uint32_t _dlssFgCapturedFrameCount = 0;
	uint32_t _dlssFgPresentedFrameCount = 0;
	uint32_t _dlssFgGeneratedPublishSuccess = 0;
	uint32_t _dlssFgGeneratedPublishFailure = 0;
	uint32_t _dlssFgRealPublishSuccess = 0;
	uint32_t _dlssFgRealPublishFailure = 0;
	bool _dlssFgPresentationStopping = false;
	bool _isXeSSFrameGenerationActive = false;
	FrameGenerationEffectKind _xessFrameGenerationKind =
		FrameGenerationEffectKind::None;
	MotionVectorRequest _xessMotionRequest{};

	// INVALID_HANDLE_VALUE 表示后端初始化失败
	std::atomic<HANDLE> _sharedTextureHandle{ NULL };
	// 下面四个成员由 _sharedTextureHandle 同步
	winrt::Windows::System::DispatcherQueue _backendThreadDispatcher{ nullptr };
	ScalingError _backendInitError = ScalingError::NoError;
	std::string _backendInitContext;
	std::vector<std::pair<std::string, MotionVectorRequest>> _motionConsumers;
	std::wstring _motionConfigurationNotice;
	std::vector<EffectDesc> _effectDescs;
	// 包含追加的 Bicubic
	std::vector<const EffectDesc*> _activeEffectDescs;
	std::vector<std::vector<EffectParameterRuntimeInfo>>
		_effectParameterRuntimeInfos;

	std::mutex _effectParameterMailboxMutex;
	std::unordered_map<uint64_t, PendingEffectParameterUpdate>
		_effectParameterMailbox;
	bool _effectParameterWakeQueued = false;
};

}
