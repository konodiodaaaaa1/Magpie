#include "pch.h"
#include "FileDialogHelper.h"
#include "Logger.h"
#include "App.h"
#include "MainWindow.h"
#include "ErrorService.h"

using namespace ::Magpie;
using namespace winrt::Magpie::implementation;
using namespace winrt;

namespace Magpie {

// 出错返回 nullopt，取消返回空字符串
std::optional<std::filesystem::path> FileDialogHelper::OpenFileDialog(
	IFileDialog* fileDialog,
	FILEOPENDIALOGOPTIONS options
) noexcept {
	FILEOPENDIALOGOPTIONS oldOptions{};
	fileDialog->GetOptions(&oldOptions);
	fileDialog->SetOptions(oldOptions | options | FOS_FORCEFILESYSTEM);

	const HRESULT showResult = fileDialog->Show(App::Get().MainWindow().Handle());
	if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::filesystem::path{};
	if (FAILED(showResult)) {
		ErrorService::Get().Report(ScalingError::FileDialogFailed, "IFileDialog::Show", nullptr,
			static_cast<uint32_t>(showResult));
		return std::nullopt;
	}

	com_ptr<IShellItem> file;
	HRESULT hr = fileDialog->GetResult(file.put());
	if (FAILED(hr)) {
		Logger::Get().ComError("IFileSaveDialog::GetResult 失败", hr);
		ErrorService::Get().Report(ScalingError::FileDialogFailed, "IFileSaveDialog::GetResult", nullptr,
			static_cast<uint32_t>(hr));
		return std::nullopt;
	}

	wil::unique_cotaskmem_string fileName;
	hr = file->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, fileName.put());
	if (FAILED(hr)) {
		Logger::Get().ComError("IShellItem::GetDisplayName 失败", hr);
		ErrorService::Get().Report(ScalingError::FileDialogFailed, "IShellItem::GetDisplayName", nullptr,
			static_cast<uint32_t>(hr));
		return std::nullopt;
	}

	return std::filesystem::path(fileName.get());
}

}
