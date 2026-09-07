#pragma once
#include "NativeEffectBackend.h"
#include "GroupBEffectProtocol.h"

namespace Magpie {

class DeviceResources;

class FSR2Upscaler final : public NativeEffectBackend {
public:
	FSR2Upscaler() = default;
	FSR2Upscaler(const FSR2Upscaler&) = delete;
	FSR2Upscaler& operator=(const FSR2Upscaler&) = delete;
	~FSR2Upscaler() override;

	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output,
		MotionVectorRequest motionRequest = {}) noexcept;
	bool Resize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	void SetFsrHdrProtocol(const FsrHdrProtocol& protocol) noexcept {
		_hdrProtocol = protocol;
	}
	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept override {
		FrameGuidanceRequirements result{ .zero = true };
		result.Add(_motionRequest);
		return result;
	}

	EffectParameterRestartReason GetParameterRestartReason(std::string_view) const noexcept override {
		return EffectParameterRestartReason::FrameGuidance;
	}

private:
	MotionVectorRequest _motionRequest{};
	void _Reset() noexcept;

	ID3D11Device* _device = nullptr;
	ID3D11DeviceContext4* _d3dDC = nullptr;
	HMODULE _coreModule = nullptr;
	HMODULE _backendModule = nullptr;
	void* _context = nullptr;
	void* _scratch = nullptr;
	size_t _scratchSize = 0;
	void* _contextCreate = nullptr;
	void* _contextDestroy = nullptr;
	void* _contextDispatch = nullptr;
	void* _getInterface = nullptr;
	void* _getScratchSize = nullptr;
	void* _getDevice = nullptr;
	void* _getResource = nullptr;
	winrt::com_ptr<ID3D11Texture2D> _zeroMotion;
	winrt::com_ptr<ID3D11UnorderedAccessView> _zeroMotionUav;
	winrt::com_ptr<ID3D11Texture2D> _zeroDepth;
	winrt::com_ptr<ID3D11UnorderedAccessView> _zeroDepthUav;
	winrt::com_ptr<ID3D11Texture2D> _reactive;
	winrt::com_ptr<ID3D11UnorderedAccessView> _reactiveUav;
	winrt::com_ptr<ID3D11Texture2D> _exposure;
	winrt::com_ptr<ID3D11UnorderedAccessView> _exposureUav;
	bool _resetHistory = true;
	FrameGuidanceFrameId _lastGuidanceResetFrameId = std::numeric_limits<FrameGuidanceFrameId>::max();
	bool _enableOpticalFlow = false;
	FsrHdrProtocol _hdrProtocol{};
};

}
