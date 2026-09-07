#pragma once
#include "NativeEffectBackend.h"
#include "GroupBEffectProtocol.h"

namespace Magpie {

class DeviceResources;

struct DLSSSRSettings {
	MotionVectorRequest motionRequest = MotionVectorRequest::Nvidia(
		NvidiaOpticalFlowQuality::Balanced);
};

// DLSS SR adapter for captured colour frames, with shared optical flow
// and a zero-depth contract.
class DLSSSRUpscaler final : public NativeEffectBackend {
public:
	DLSSSRUpscaler() = default;
	DLSSSRUpscaler(const DLSSSRUpscaler&) = delete;
	DLSSSRUpscaler& operator=(const DLSSSRUpscaler&) = delete;
	~DLSSSRUpscaler() override;

	bool Initialize(
		DeviceResources& deviceResources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const DLSSSRSettings& settings = {}
	) noexcept;

	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept override;
	EffectParameterApplyMode GetParameterApplyMode(
		std::string_view /*parameterName*/
	) const noexcept override {
		return EffectParameterApplyMode::RestartRequired;
	}
	EffectParameterRestartReason GetParameterRestartReason(
		std::string_view parameterName
	) const noexcept override {
		return parameterName == "opticalFlowMethod" ||
			parameterName == "amdOpticalFlowMode" || parameterName == "nvidiaOpticalFlowQuality"
			? EffectParameterRestartReason::FrameGuidance
			: EffectParameterRestartReason::NativeBackend;
	}

	bool Resize(
		DeviceResources& deviceResources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output
	) noexcept override;

	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	void SetDlssHdrProtocol(const FsrHdrProtocol& protocol) noexcept { _hdrProtocol = protocol; }

private:
	void _Reset() noexcept;

	ID3D11Device5* _device = nullptr;
	ID3D11DeviceContext4* _d3dDC = nullptr;
	winrt::com_ptr<ID3D11Texture2D> _zeroMotionVectors;
	winrt::com_ptr<ID3D11UnorderedAccessView> _zeroMotionVectorsUav;
	winrt::com_ptr<ID3D11Texture2D> _zeroDepth;
	winrt::com_ptr<ID3D11UnorderedAccessView> _zeroDepthUav;
	winrt::com_ptr<ID3D11Texture2D> _biasCurrentColorMask;
	winrt::com_ptr<ID3D11UnorderedAccessView> _biasCurrentColorMaskUav;
	void* _parameters = nullptr;
	void* _feature = nullptr;
	bool _ngxInitialized = false;
	bool _resetHistory = true;
	DLSSSRSettings _settings{};
	uint8_t _lastGuidanceBinding = UINT8_MAX;
	FrameGuidanceFrameId _lastGuidanceResetFrameId =
		std::numeric_limits<FrameGuidanceFrameId>::max();
	FsrHdrProtocol _hdrProtocol{};
};

}
