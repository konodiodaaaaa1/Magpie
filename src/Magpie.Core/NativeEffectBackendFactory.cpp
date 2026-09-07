#include "pch.h"
#include "NativeEffectBackendFactory.h"
#include "NgxD3D12Core.h"
#include "DLSSNRFilter.h"
#include "DLSSSRUpscaler.h"
#include "FSR2ZeroMVUpscaler.h"
#include "FSR3ZeroMVUpscaler.h"
#include "FSR2Upscaler.h"
#include "FSR3Upscaler.h"
#include "RTXVideoDenoiser.h"
#include "XeSSZeroMVUpscaler.h"
#include "XeSSUpscaler.h"
#include "FrameGuidanceDiagnostics.h"
#include "Logger.h"
#include "ScalingOptions.h"
#include "ScalingWindow.h"
#include "EffectParameterRules.h"
#include "OpticalFlowSettings.h"

namespace Magpie {

template <typename T, typename... Args>
static NativeEffectBackendResult CreateBackend(
	std::string_view displayName,
	DeviceResources& resources,
	ID3D11Texture2D* input,
	ID3D11Texture2D* output,
	Args&&... args
) noexcept {
	auto backend = std::make_unique<T>();
	if (!backend->Initialize(
		resources, input, output, std::forward<Args>(args)...)) {
		Logger::Get().Error(fmt::format("Initialize native effect {} failed", displayName));
		return { true, nullptr };
	}
	return { true, std::move(backend) };
}

NativeEffectBackendResult CreateNativeEffectBackend(
	std::string_view effectName,
	const EffectOption& option,
	DeviceResources& resources,
	NgxD3D12Core& ngxCore,
	ID3D11Texture2D* input,
	ID3D11Texture2D* output
) noexcept {
	const bool hdrEnabled = ScalingWindow::Get().Options().IsHdrCompatibilityEnabled();
	if (effectName == "Diagnostics\\FrameGuidance_Motion" ||
		effectName == "Diagnostics\\FrameGuidance_Confidence") {
		auto getParameter = [&](std::string_view name, float defaultValue) {
			auto it = option.parameters.find(std::string(name));
			return it == option.parameters.end() ? defaultValue : it->second;
		};
		FrameGuidanceDiagnosticKind kind =
			FrameGuidanceDiagnosticKind::Motion;
		if (effectName.ends_with("Confidence")) {
			kind = FrameGuidanceDiagnosticKind::Confidence;
		}
		return CreateBackend<FrameGuidanceDiagnostics>(
			effectName, resources, input, output,
			FrameGuidanceDiagnosticSettings{
				.kind = kind,
				.gain = std::max(0.001f, getParameter("gain",
					FrameGuidanceDiagnosticSettings{}.gain))
			});
	}

	if (effectName == "DLSSNR\\DLSSNR_AI_Filter") {
		const DLSSNRSettings settings = ParseDLSSNRSettings(option, hdrEnabled);
		auto backend = std::make_unique<DLSSNRFilter>();
		if (!backend->Initialize(resources, ngxCore, input, output, settings)) {
			const char status[] =
				"DLSSNR STATUS: Feature=18 created=false path=unavailable "
				"fallback=pass-through\n";
			Logger::Get().Warn(status);
			OutputDebugStringA(status);
			return {};
		}
		return { true, std::move(backend) };
	}


	if (!hdrEnabled) {
		if (IsSuperResolutionEffect(effectName)) {
			const auto motion = ParseOpticalFlowRequest(option,
				effectName == "DLSS\\DLSS_SR" ? OpticalFlowMethod::Nvidia : OpticalFlowMethod::None);
			if (effectName == "DLSS\\DLSS_SR")
				return CreateBackend<DLSSSRUpscaler>(effectName, resources, input, output,
					DLSSSRSettings{ .motionRequest = motion });
			if (effectName == "FSR2\\FSR2_SR")
				return CreateBackend<FSR2Upscaler>(effectName, resources, input, output, motion);
			if (effectName == "XeSS\\XeSS_SR")
				return CreateBackend<XeSSUpscaler>(effectName, resources, input, output, motion);
			return CreateBackend<FSR3Upscaler>(effectName, resources, input, output,
				motion, effectName == "FSR4\\FSR4_SR");
		}
	}

	// Keep the legacy ZeroMV/Jitter/OpticalFlow contracts on their original
	// backends. These names carry distinct temporal and auxiliary-resource
	// semantics even when HDR compatibility is disabled.
	if (hdrEnabled && (effectName == "DLSS\\DLSS_ZeroMV" ||
		effectName == "DLSS\\DLSS_ZeroMV_Jitter" ||
		effectName == "DLSS\\DLSS_OpticalFlow")) {
		auto getParameter = [&](std::string_view name, float defaultValue) {
			auto it = option.parameters.find(std::string(name));
			return it == option.parameters.end() ? defaultValue : it->second;
		};
		const bool isLegacyOpticalFlow = effectName == "DLSS\\DLSS_OpticalFlow";
		const auto quality = static_cast<NvidiaOpticalFlowQuality>(std::clamp(
			static_cast<int>(std::lround(getParameter("nvidiaOpticalFlowQuality", 2.0f))),
			0, int(NVIDIA_OPTICAL_FLOW_MAX_QUALITY)));
		return CreateBackend<DLSSSRUpscaler>(
			effectName, resources, input, output,
			DLSSSRSettings{
				.motionRequest = isLegacyOpticalFlow
					? MotionVectorRequest::Nvidia(quality)
					: MotionVectorRequest{}
			});
	}

	if (hdrEnabled && (effectName == "FSR2\\FSR2_ZeroMV" ||
		effectName == "FSR2\\FSR2_ZeroMV_Jitter" ||
		effectName == "FSR2\\FSR2_OpticalFlow")) {
		return CreateBackend<FSR2ZeroMVUpscaler>(effectName, resources, input, output,
			effectName == "FSR2\\FSR2_OpticalFlow",
			effectName == "FSR2\\FSR2_ZeroMV_Jitter");
	}

	const bool isFsr3 = effectName == "FSR3\\FSR3_ZeroMV" ||
		effectName == "FSR3\\FSR3_ZeroMV_Jitter" ||
		effectName == "FSR3\\FSR3_OpticalFlow";
	const bool isFsr4 = effectName == "FSR4\\FSR4_ZeroMV" ||
		effectName == "FSR4\\FSR4_ZeroMV_Jitter" ||
		effectName == "FSR4\\FSR4_OpticalFlow";
	if (hdrEnabled && (isFsr3 || isFsr4)) {
		return CreateBackend<FSR3ZeroMVUpscaler>(effectName, resources, input, output,
			effectName.ends_with("OpticalFlow"),
			effectName.ends_with("ZeroMV_Jitter"), isFsr4);
	}

	if (hdrEnabled && (effectName == "XeSS\\XeSS_ZeroMV" ||
		effectName == "XeSS\\XeSS_ZeroMV_Jitter" ||
		effectName == "XeSS\\XeSS_OpticalFlow")) {
		return CreateBackend<XeSSZeroMVUpscaler>(effectName, resources, input, output,
			effectName == "XeSS\\XeSS_OpticalFlow",
			effectName == "XeSS\\XeSS_ZeroMV_Jitter");
	}

	if (hdrEnabled && (effectName == "DLSS\\DLSS_SR" ||
		effectName == "FSR2\\FSR2_SR" ||
		effectName == "FSR3\\FSR3_SR" ||
		effectName == "FSR4\\FSR4_SR" ||
		effectName == "XeSS\\XeSS_SR")) {
		const auto motion = ParseOpticalFlowRequest(option,
			effectName == "DLSS\\DLSS_SR" ? OpticalFlowMethod::Nvidia : OpticalFlowMethod::None);
		const D3D11_TEXTURE2D_DESC inputDesc = [&]() {
			D3D11_TEXTURE2D_DESC desc{};
			input->GetDesc(&desc);
			return desc;
		}();
		const bool hdrInput = hdrEnabled &&
			inputDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT;
		const FsrHdrProtocol hdrProtocol{
			.hdrColorInput = hdrInput,
			.transfer = GroupBTransfer::Linear,
			.preExposure = 1.0f,
			.exposure = 1.0f,
			.depthInverted = true,
			.depthInfinite = true,
			.useReactiveMask = true,
			.useTransparencyMask = true,
		};
		if (effectName == "DLSS\\DLSS_SR") {
			auto backend = std::make_unique<DLSSSRUpscaler>();
			backend->SetDlssHdrProtocol(hdrProtocol);
			if (!backend->Initialize(resources, input, output,
				DLSSSRSettings{ .motionRequest = motion })) return { true, nullptr };
			return { true, std::move(backend) };
		}
		if (effectName == "FSR2\\FSR2_SR") {
			auto backend = std::make_unique<FSR2Upscaler>();
			backend->SetFsrHdrProtocol(hdrProtocol);
			if (!backend->Initialize(resources, input, output, motion)) return { true, nullptr };
			return { true, std::move(backend) };
		}
		if (effectName == "XeSS\\XeSS_SR")
			return CreateBackend<XeSSUpscaler>(effectName, resources, input, output, motion);
		auto backend = std::make_unique<FSR3Upscaler>();
		backend->SetFsrHdrProtocol(hdrProtocol);
		if (!backend->Initialize(resources, input, output, motion,
			effectName == "FSR4\\FSR4_SR")) return { true, nullptr };
		return { true, std::move(backend) };
	}

	const bool isRtxVideo = effectName.starts_with("RTXVideo\\RTXVideo_Denoise_") ||
		effectName.starts_with("RTXVideo\\RTXVideo_VSR_");
	if (isRtxVideo) {
		const bool isVsr = effectName.find("_VSR_") != std::string_view::npos;
		uint32_t qualityLevel = 8;
		if (isVsr) {
			qualityLevel = effectName.ends_with("_Low") ? 1 :
				effectName.ends_with("_Medium") ? 2 :
				effectName.ends_with("_High") ? 3 : 4;
		} else if (effectName.ends_with("_Medium")) {
			qualityLevel = 9;
		} else if (effectName.ends_with("_High")) {
			qualityLevel = 10;
		} else if (effectName.ends_with("_Ultra")) {
			qualityLevel = 11;
		}
		auto backend = std::make_unique<RTXVideoDenoiser>();
		if (!backend->Initialize(resources, input, output, qualityLevel,
			isVsr ? RtxVideoEffectKind::Vsr : RtxVideoEffectKind::Denoise)) {
			Logger::Get().Error(fmt::format("Initialize native effect {} failed", effectName));
			return { true, nullptr, backend->InitializationError() };
		}
		return { true, std::move(backend) };
	}
	return {};
}

}
