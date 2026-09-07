#include "pch.h"
#include "ErrorService.h"
#include "App.h"
#include "CommonSharedConstants.h"
#include "Logger.h"
#include "StrHelper.h"
#include "ToastService.h"
#include "Win32Helper.h"

namespace Magpie {

static const wchar_t* MessageKey(ScalingError error) noexcept {
	switch (error) {
	case ScalingError::InvalidScalingMode: return L"Message_InvalidScalingMode";
	case ScalingError::TouchSupport: return L"Message_TouchSupport";
	case ScalingError::Windowed3DGameMode: return L"Message_Windowed3DGameMode";
	case ScalingError::WindowedDesktopDuplication: return L"Message_WindowedDesktopDuplication";
	case ScalingError::InvalidSourceWindow: return L"Message_InvalidSourceWindow";
	case ScalingError::Maximized: return L"Message_Maximized";
	case ScalingError::LowIntegrityLevel: return L"Message_LowIntegrityLevel";
	case ScalingError::InvalidCropping: return L"Message_InvalidCropping";
	case ScalingError::BannedInWindowedMode: return L"Message_BannedInWindowedMode";
	case ScalingError::ScalingFailedGeneral: return L"Message_ScalingFailedGeneral";
	case ScalingError::CaptureFailed: return L"Message_CaptureFailed";
	case ScalingError::CreateFenceFailed: return L"Message_CreateFenceFailed";
	case ScalingError::NvidiaVsrPathUnsupported: return L"Message_NvidiaVsrPathUnsupported";
	case ScalingError::OpticalFlowProviderUnavailable: return L"Message_OpticalFlowProviderUnavailable";
	case ScalingError::NvidiaOpticalFlowUnsupported: return L"Message_NvidiaOpticalFlowUnsupported";
	case ScalingError::NvidiaOpticalFlowQualityUnsupported: return L"Message_NvidiaOpticalFlowQualityUnsupported";
	case ScalingError::AmdOpticalFlowUnsupported: return L"Message_AmdOpticalFlowUnsupported";
	case ScalingError::OpticalFlowInteropFailed: return L"Message_OpticalFlowInteropFailed";
	case ScalingError::ConflictingFrameGenerationEffects: return L"Message_ConflictingFrameGenerationEffects";
	case ScalingError::XeSSMfgRequiresIntel: return L"Message_XeSSMfgRequiresIntel";
	case ScalingError::XeSSMfgUnsupported: return L"Message_XeSSMfgUnsupported";
	case ScalingError::XeSSMfgMultiplierUnsupported: return L"Message_XeSSMfgMultiplierUnsupported";
	case ScalingError::ScalingModeNotSelected: return L"Message_ScalingModeNotSelected";
	case ScalingError::ScalingModeEmpty: return L"Message_ScalingModeEmpty";
	case ScalingError::ScalingModeUnknownEffect: return L"Message_ScalingModeUnknownEffect";
	case ScalingError::GraphicsDeviceInitFailed: return L"Message_GraphicsDeviceInitFailed";
	case ScalingError::PresentationInitFailed: return L"Message_PresentationInitFailed";
	case ScalingError::EffectCompileFailed: return L"Message_EffectCompileFailed";
	case ScalingError::EffectResourceFailed: return L"Message_EffectResourceFailed";
	case ScalingError::NativeEffectInitFailed: return L"Message_NativeEffectInitFailed";
	case ScalingError::FrameGenerationInitFailed: return L"Message_FrameGenerationInitFailed";
	case ScalingError::OverlayInitFailed: return L"Message_OverlayInitFailed";
	case ScalingError::SharedTextureOpenFailed: return L"Message_SharedTextureOpenFailed";
	case ScalingError::DlssNrUnavailable: return L"Message_DlssNrUnavailable";
	case ScalingError::FrameGenerationDisabled: return L"Message_FrameGenerationDisabled";
	case ScalingError::ConfigurationWriteFailed: return L"Message_ConfigurationWriteFailed";
	case ScalingError::EffectParameterConflict: return L"Message_EffectParameterConflict";
	case ScalingError::EffectParameterLiveFailed: return L"Message_EffectParameterLiveFailed";
	case ScalingError::ScreenshotDirectoryFailed: return L"Message_ScreenshotDirectoryFailed";
	case ScalingError::ScreenshotEncodeFailed: return L"Message_ScreenshotEncodeFailed";
	case ScalingError::ScreenshotWriteFailed: return L"Message_ScreenshotWriteFailed";
	case ScalingError::ScreenshotReadbackFailed: return L"Message_ScreenshotReadbackFailed";
	case ScalingError::SourceWindowClosed: return L"Message_SourceWindowClosed";
	case ScalingError::SourceWindowUnresponsive: return L"Message_SourceWindowUnresponsive";
	case ScalingError::SourceWindowTooSmall: return L"Message_SourceWindowTooSmall";
	case ScalingError::SourceWindowOffscreen: return L"Message_SourceWindowOffscreen";
	case ScalingError::SourceWindowUnsupported: return L"Message_SourceWindowUnsupported";
	case ScalingError::SourceWindowGeometryFailed: return L"Message_SourceWindowGeometryFailed";
	case ScalingError::ScalingAlreadyActive: return L"Message_ScalingAlreadyActive";
	case ScalingError::ScalingWindowCreationFailed: return L"Message_ScalingWindowCreationFailed";
	case ScalingError::DisplayLayoutFailed: return L"Message_DisplayLayoutFailed";
	case ScalingError::ImportReadFailed: return L"Message_ImportReadFailed";
	case ScalingError::ImportEmpty: return L"Message_ImportEmpty";
	case ScalingError::ImportInvalidJson: return L"Message_ImportInvalidJson";
	case ScalingError::ImportWrongFileType: return L"Message_ImportWrongFileType";
	case ScalingError::ImportIncompatible: return L"Message_ImportIncompatible";
	case ScalingError::ExportWriteFailed: return L"Message_ExportWriteFailed";
	case ScalingError::FileDialogFailed: return L"Message_FileDialogFailed";
	case ScalingError::PassThroughUnavailable: return L"Message_PassThroughUnavailable";
	default: return L"Message_ScalingFailedGeneral";
	}
}

void ErrorService::DismissIssue() {
	if (!_isIssueVisible) return;
	_isIssueVisible = false;
	Changed.Invoke();
}

void ErrorService::Report(ScalingError error, std::string context, HWND target,
	uint32_t systemError) noexcept {
	if (error == ScalingError::NoError) return;
	try {
		Logger::Get().Error(fmt::format("Diagnostic MP-{:03}: system={} context={}",
			static_cast<int>(error), systemError, context));
		Logger::Get().Flush();
		winrt::Magpie::implementation::App::Get().Dispatcher().TryEnqueue([this, error, context = std::move(context), target, systemError] {
			try {
				const auto loader = winrt::ResourceLoader::GetForViewIndependentUse(
					CommonSharedConstants::APP_RESOURCE_MAP_ID);
				const auto now = std::chrono::steady_clock::now();
				const bool repeated = _lastError == error && _lastContext == context &&
					_lastSystemError == systemError;
				_repeatCount = repeated ? _repeatCount + 1 : 1;
				_lastError = error;
				_lastContext = context;
				_lastSystemError = systemError;
				_summary = loader.GetString(MessageKey(error));
				std::wstring advice;
				const uint32_t win32Error = HRESULT_FACILITY(systemError) == FACILITY_WIN32
					? HRESULT_CODE(systemError) : systemError;
				if (win32Error == ERROR_ACCESS_DENIED || win32Error == ERROR_WRITE_PROTECT) {
					advice = loader.GetString(L"ErrorAdvice_AccessDenied");
				} else if (win32Error == ERROR_SHARING_VIOLATION || win32Error == ERROR_LOCK_VIOLATION) {
					advice = loader.GetString(L"ErrorAdvice_FileBusy");
				} else if (win32Error == ERROR_DISK_FULL || win32Error == ERROR_HANDLE_DISK_FULL) {
					advice = loader.GetString(L"ErrorAdvice_DiskFull");
				} else if (win32Error == ERROR_PATH_NOT_FOUND || win32Error == ERROR_FILE_NOT_FOUND) {
					advice = loader.GetString(L"ErrorAdvice_PathMissing");
				}
				SYSTEMTIME time{};
				GetLocalTime(&time);
				std::wstring details(_summary);
				if (!advice.empty()) details += L"\n\n" + advice;
				auto field = [&](std::wstring_view key, std::wstring_view value) {
					details += L"\n" + std::wstring(loader.GetString(key)) + L": " + std::wstring(value);
				};
				details += L"\n";
				field(L"ErrorDetails_Time", fmt::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
					time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond));
				field(L"ErrorDetails_Code", fmt::format(L"MP-{:03}", static_cast<int>(error)));
#ifdef MP_VERSION_STRING
				field(L"ErrorDetails_Version", StrHelper::UTF8ToUTF16(STRINGIFY(MP_VERSION_STRING)));
#endif
				if (!context.empty()) field(L"ErrorDetails_Context", StrHelper::UTF8ToUTF16(context));
				if (systemError) field(L"ErrorDetails_SystemCode", fmt::format(L"{} (0x{:08X})", systemError, systemError));
				field(L"ErrorDetails_Count", std::to_wstring(_repeatCount));
				field(L"ErrorDetails_Logs", (Win32Helper::GetExePath().parent_path() / L"logs").native());
				_details = winrt::hstring(details);
				_isIssueVisible = true;
				Changed.Invoke();
				// Do not flood a continuous save/live-update failure with popups.
				if (!repeated || now - _lastToast >= std::chrono::seconds(15)) {
					_lastToast = now;
					const std::wstring message = std::wstring(_summary) + L"\n" +
						std::wstring(loader.GetString(L"ErrorDetails_HomeHint"));
					if (target && IsWindow(target)) {
						ToastService::Get().ShowMessageOnWindow({}, message, target, std::chrono::seconds(7));
					} else {
						ToastService::Get().ShowMessageInApp({}, message, std::chrono::seconds(7));
					}
				}
			} catch (...) {
				Logger::Get().Error("Unable to present diagnostic; original error was logged");
			}
		});
	} catch (...) {
		Logger::Get().Error("Unable to enqueue diagnostic");
	}
}

}
