#pragma once
#include "HdrCaptureProcessor.h"

namespace Magpie {

class DeviceResources;
class BackendDescriptorStore;

enum class FrameSourceWaitType {
	NoWait,
	WaitForMessage,
	WaitForEvent,
	WaitForFrame
};

enum class FrameSourceState {
	NewFrame,
	Waiting,
	Error
};

class FrameSourceBase {
public:
	FrameSourceBase() noexcept;

	virtual ~FrameSourceBase() noexcept;

	// 不可复制，不可移动
	FrameSourceBase(const FrameSourceBase&) = delete;
	FrameSourceBase(FrameSourceBase&&) = delete;

	bool Initialize(DeviceResources& deviceResources, BackendDescriptorStore& descriptorStore) noexcept;

	virtual bool Start() noexcept { return true; }

	FrameSourceState Update() noexcept;

	// Backend-thread state. A sequence changes only on a real capture discontinuity;
	// the first valid frame must reach temporal consumers even if its pixels match.
	uint64_t CaptureSequence() const noexcept { return _captureSequence; }
	uint64_t ResourceGeneration() const noexcept { return _resourceGeneration; }
	bool IsCaptureInterrupted() const noexcept { return _captureInterrupted; }
	int64_t CaptureTimestamp100ns() const noexcept { return _captureTimestamp100ns; }
	const char* CaptureErrorContext() const noexcept { return _captureErrorContext; }
	HRESULT CaptureErrorCode() const noexcept { return _captureErrorCode; }

	void ForceDuplicateFrameDetection(bool value) noexcept {
		_forceDuplicateFrameDetection = value;
	}

	// Compatibility getter. HDR callers must use GetCanonicalFrame(); SDR keeps
	// the original raw capture texture contract.
	ID3D11Texture2D* GetOutput() noexcept { return _hdrEnabled ?
		_hdrProcessor.GetCanonicalTexture() : _output.get(); }

	ID3D11Texture2D* GetPipelineTexture() const noexcept {
		return _hdrEnabled ? _hdrProcessor.GetCanonicalTexture() : _output.get();
	}

	HdrFrame GetCanonicalFrame() const noexcept {
		if (!_hdrEnabled) return {};
		HdrFrame frame{
			.texture = _hdrProcessor.GetCanonicalTexture(),
			.metadata = _hdrProcessor.GetFrameMetadata(),
			.workingFormat = DXGI_FORMAT_R16G16B16A16_FLOAT
		};
		return frame;
	}

	// Raw capture is an internal capture-front-end input. Renderer/effect code
	// must use GetCanonicalFrame() in HDR mode.
	ID3D11Texture2D* GetRawCaptureTexture() const noexcept { return _output.get(); }

	const HdrFrameMetadata& GetHdrFrameMetadata() const noexcept {
		return _hdrProcessor.GetFrameMetadata();
	}

	bool PrepareHdrOutputForResize() noexcept;
	bool IsHdrFrameReady() const noexcept { return !_hdrEnabled || _hdrFrameReady; }

	std::pair<uint32_t, uint32_t> GetStatisticsForDynamicDetection() const noexcept;

	virtual const char* Name() const noexcept = 0;

	virtual FrameSourceWaitType WaitType() const noexcept = 0;
	// Borrowed on the backend thread; valid until its next capture operation.
	virtual HANDLE FrameArrivedEvent() const noexcept { return nullptr; }
	
	virtual void OnCursorVisibilityChanged(bool /*isVisible*/, bool /*onDestory*/) noexcept {};

protected:
	virtual ColorDescription _GetSourceColorDescription() const noexcept;
	uint64_t _captureSequence = 0;
	bool _captureInterrupted = false;
	int64_t _captureTimestamp100ns = 0;
	const char* _captureErrorContext = "Capture frame update";
	HRESULT _captureErrorCode = S_OK;

	virtual bool _Initialize() noexcept = 0;

	virtual FrameSourceState _Update() noexcept = 0;

	void _DisableRoundCornerInWin11() noexcept;

	// 获取坐标系 1 到坐标系 2 的映射关系
	// 坐标系 1: 屏幕坐标系，即虚拟化后的坐标系。原点为屏幕左上角
	// 坐标系 2: 虚拟化前的坐标系，即源窗口所见的坐标系，原点为窗口左上角
	// 两坐标系为线性映射，a 和 b 返回该映射的参数
	// 如果窗口本身支持高 DPI，则 a 为 1，否则 a 为 DPI 缩放的倒数
	// 此函数是为了将屏幕上的点映射到窗口坐标系中，并且无视 DPI 虚拟化
	// 坐标系 1 中的 (x1, y1) 映射到 (x1 * a + bx, x2 * a + by)
	static bool _GetMapToOriginDPI(HWND hWnd, double& a, double& bx, double& by) noexcept;

	DeviceResources* _deviceResources = nullptr;
	BackendDescriptorStore* _descriptorStore = nullptr;
	winrt::com_ptr<ID3D11Texture2D> _output;
	HdrCaptureProcessor _hdrProcessor;
	bool _hdrEnabled = false;
	bool _hdrFrameReady = false;
	uint64_t _hdrFrameSequence = 0;
	uint64_t _resourceGeneration = 1;
	bool _hdrDiagnosticsLogged = false;
	ID3D11ShaderResourceView* _outputSrv = nullptr;

	winrt::com_ptr<ID3D11Buffer> _resultBuffer;
	ID3D11UnorderedAccessView* _resultBufferUav = nullptr;
	winrt::com_ptr<ID3D11Buffer> _readBackBuffer;
	winrt::com_ptr<ID3D11ComputeShader> _dupFrameCS;
	std::pair<uint32_t, uint32_t> _dispatchCount;

private:
	uint64_t _duplicateCaptureSequence = 0;
	uint32_t _duplicateReadbackSamples = 0;
	double _duplicateReadbackTotalMs = 0;
	double _duplicateReadbackMaxMs = 0;

	bool _InitCheckingForDuplicateFrame();

	bool _IsDuplicateFrame();

	// (预测错误帧数, 总计跳过帧数)
	std::atomic<std::pair<uint32_t, uint32_t>> _statistics;

	// 用于检查重复帧
	winrt::com_ptr<ID3D11Texture2D> _prevFrame;
	winrt::com_ptr<ID3D11ShaderResourceView> _prevFrameSrv;
	uint16_t _nextSkipCount;
	uint16_t _framesLeft;
	
	bool _isCheckingForDuplicateFrame = true;
	bool _forceDuplicateFrameDetection = false;

protected:
	bool _roundCornerDisabled = false;
};

}
