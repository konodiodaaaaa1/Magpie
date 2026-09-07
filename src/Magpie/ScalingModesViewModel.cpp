#include "pch.h"
#include "ScalingModesViewModel.h"
#if __has_include("ScalingModesViewModel.g.cpp")
#include "ScalingModesViewModel.g.cpp"
#endif
#include "App.h"
#include "AppSettings.h"
#include "CommonSharedConstants.h"
#include "FileDialogHelper.h"
#include "Logger.h"
#include "ScalingMode.h"
#include "ScalingModeItem.h"
#include "ToastService.h"
#include "Win32Helper.h"
#include "ErrorService.h"
#include "StrHelper.h"
#include <rapidjson/error/en.h>

using namespace Magpie;

namespace winrt::Magpie::implementation {

ScalingModesViewModel::ScalingModesViewModel() {
	_scalingModesChangedRevoker = _scalingModes.VectorChanged(
		auto_revoke, { this, &ScalingModesViewModel::_ScalingModes_VectorChanged });

	_AddScalingModes();

	_scalingModeAddedRevoker = ScalingModesService::Get().ScalingModeAdded(
		auto_revoke, std::bind_front(&ScalingModesViewModel::_ScalingModesService_Added, this));
	_scalingModeMovedRevoker = ScalingModesService::Get().ScalingModeMoved(
		auto_revoke, std::bind_front(&ScalingModesViewModel::_ScalingModesService_Moved, this));
	_scalingModeRemovedRevoker = ScalingModesService::Get().ScalingModeRemoved(
		auto_revoke, std::bind_front(&ScalingModesViewModel::_ScalingModesService_Removed, this));
	_scalingModesResetRevoker = ScalingModesService::Get().ScalingModesReset(
		auto_revoke, std::bind_front(&ScalingModesViewModel::_ScalingModesService_Reset, this));
}

static std::optional<std::filesystem::path> OpenFileDialogForJson(
	IFileDialog* fileDialog,
	const wchar_t* title,
	const wchar_t* jsonFileStr
) noexcept {
	fileDialog->SetTitle(title);
	const COMDLG_FILTERSPEC fileType{ jsonFileStr, L"*.json" };
	fileDialog->SetFileTypes(1, &fileType);
	fileDialog->SetDefaultExtension(L"json");

	return FileDialogHelper::OpenFileDialog(fileDialog, FOS_STRICTFILETYPES);
}

fire_and_forget ScalingModesViewModel::Export() noexcept {
	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	const hstring title = resourceLoader.GetString(L"Dialog_Export_Title");
	const hstring jsonFileStr = resourceLoader.GetString(L"Dialog_JsonFile");

	auto weakThis = get_weak();

	// 在主线程使用 IFileOpenDialog 有些问题，尤其在 Win10 中
	co_await resume_background();

	com_ptr<IFileSaveDialog> fileDialog = try_create_instance<IFileSaveDialog>(CLSID_FileSaveDialog);
	if (!fileDialog) {
		Logger::Get().Error("创建 FileSaveDialog 失败");
		ErrorService::Get().Report(ScalingError::FileDialogFailed, "Create FileSaveDialog");
		co_return;
	}

	fileDialog->SetFileName(L"ScalingModes");

	std::optional<std::filesystem::path> fileName =
		OpenFileDialogForJson(fileDialog.get(), title.c_str(), jsonFileStr.c_str());
	if (!fileName.has_value() || fileName->empty()) {
		co_return;
	}

	co_await App::Get().Dispatcher();

	if (!weakThis.get()) {
		co_return;
	}

	rapidjson::StringBuffer json;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(json);
	writer.StartObject();
	ScalingModesService::Get().Export(writer);
	writer.EndObject();

	if (!Win32Helper::WriteTextFile(fileName->c_str(), { json.GetString(), json.GetLength() })) {
		ErrorService::Get().Report(ScalingError::ExportWriteFailed,
			StrHelper::UTF16ToUTF8(fileName->native()));
	}
}

fire_and_forget ScalingModesViewModel::Import() {
	const ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	const hstring title = resourceLoader.GetString(L"Dialog_Import_Title");
	const hstring jsonFileStr = resourceLoader.GetString(L"Dialog_JsonFile");

	auto weakThis = get_weak();

	// 在主线程使用 IFileOpenDialog 有些问题，尤其在 Win10 中
	co_await resume_background();

	com_ptr<IFileOpenDialog> fileDialog = try_create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);
	if (!fileDialog) {
		Logger::Get().Error("创建 FileOpenDialog 失败");
		ErrorService::Get().Report(ScalingError::FileDialogFailed, "Create FileOpenDialog");
		co_return;
	}

	std::optional<std::filesystem::path> fileName =
		OpenFileDialogForJson(fileDialog.get(), title.c_str(), jsonFileStr.c_str());
	if (!fileName.has_value()) {
		co_return;
	}
	if (fileName->empty()) {
		co_return;
	}

	std::string json;
	if (!Win32Helper::ReadTextFile(fileName->c_str(), json)) {
		ErrorService::Get().Report(ScalingError::ImportReadFailed,
			StrHelper::UTF16ToUTF8(fileName->native()));
		co_return;
	}

	co_await App::Get().Dispatcher();

	if (!weakThis.get()) {
		co_return;
	}

	const std::string path = StrHelper::UTF16ToUTF8(fileName->native());
	if (json.find_first_not_of(" \t\r\n") == std::string::npos) {
		ErrorService::Get().Report(ScalingError::ImportEmpty, path);
		co_return;
	}
	rapidjson::Document doc;
	// Preserve the existing acceptance of comments and trailing commas.
	doc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(json.data(), json.size());
	if (doc.HasParseError()) {
		ErrorService::Get().Report(ScalingError::ImportInvalidJson,
			fmt::format("{} / byte {} / {}", path, doc.GetErrorOffset(),
				rapidjson::GetParseError_En(doc.GetParseError())));
		co_return;
	}
	if (!doc.IsObject() || !doc.HasMember("scalingModes") || !doc["scalingModes"].IsArray()) {
		ErrorService::Get().Report(ScalingError::ImportWrongFileType, path);
		co_return;
	}
	if (doc["scalingModes"].Empty()) {
		ErrorService::Get().Report(ScalingError::ImportEmpty, path);
		co_return;
	}
	if (!ScalingModesService::Get().Import(((const rapidjson::Document&)doc).GetObj(), false)) {
		ErrorService::Get().Report(ScalingError::ImportIncompatible, path);
	}
}

bool ScalingModesViewModel::CanReorderScalingModes() const noexcept {
	return _scalingModes.Size() > 1;
}

void ScalingModesViewModel::AddScalingMode() {
	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	std::wstring baseName(resourceLoader.GetString(L"ScalingModes_NewScalingMode/Text"));
	std::wstring name = baseName;

	const auto& scalingModes = AppSettings::Get().ScalingModes();
	for (uint32_t suffix = 2;; ++suffix) {
		const bool exists = std::any_of(scalingModes.begin(), scalingModes.end(), [&name](const ScalingMode& mode) {
			return mode.name == name;
		});
		if (!exists) {
			break;
		}
		name = baseName + L" (" + std::to_wstring(suffix) + L")";
	}

	ScalingModesService::Get().AddScalingMode(name, -1);
}

fire_and_forget ScalingModesViewModel::_AddScalingModes(
	bool isInitialExpanded,
	bool shouldAutoRename) {
	_pendingInitialExpanded = _pendingInitialExpanded || isInitialExpanded;
	_pendingAutoRename = _pendingAutoRename || shouldAutoRename;

	if (_addingScalingModes) {
		co_return;
	}
	_addingScalingModes = true;
	const uint32_t collectionGeneration = _collectionGeneration;

	ScalingModesService& scalingModesService = ScalingModesService::Get();
	uint32_t total = scalingModesService.GetScalingModeCount();
	uint32_t curSize = _scalingModes.Size();

	if (total - curSize <= 5) {
		for (; curSize < total; ++curSize) {
			const bool isNewest = curSize + 1 == total;
			const bool expandNewest = isNewest &&
				std::exchange(_pendingInitialExpanded, false);
			const bool renameNewest = isNewest &&
				std::exchange(_pendingAutoRename, false);
			_updatingScalingModes = true;
			_scalingModes.Append(make<ScalingModeItem>(
				curSize,
				expandNewest,
				renameNewest));
			_updatingScalingModes = false;
		}
	} else {
		assert(!isInitialExpanded);

		// 延迟加载
		for (int j = 0; j < 5; ++j) {
			_updatingScalingModes = true;
			_scalingModes.Append(make<ScalingModeItem>(curSize++, false, false));
			_updatingScalingModes = false;
		}

		auto weakThis = get_weak();

		while (true) {
			co_await 10ms;
			co_await App::Get().Dispatcher();

			if (!weakThis.get()) {
				co_return;
			}
			if (collectionGeneration != _collectionGeneration) {
				_addingScalingModes = false;
				_AddScalingModes();
				co_return;
			}

			total = scalingModesService.GetScalingModeCount();
			curSize = _scalingModes.Size();

			if (curSize < total) {
				const bool isNewest = curSize + 1 == total;
				const bool expandNewest = isNewest &&
					std::exchange(_pendingInitialExpanded, false);
				const bool renameNewest = isNewest &&
					std::exchange(_pendingAutoRename, false);
				_updatingScalingModes = true;
				_scalingModes.Append(make<ScalingModeItem>(
					curSize++, expandNewest, renameNewest));
				_updatingScalingModes = false;
			}
			
			if (curSize >= total) {
				break;
			}
		}
	}

	_addingScalingModes = false;
	RaisePropertyChanged(L"CanReorderScalingModes");
}

void ScalingModesViewModel::_ScalingModesService_Added(EffectAddedWay way) {
	// 不支持在事件回调中修改事件本身，因此延迟执行
	App::Get().Dispatcher().TryEnqueue([this, way]() {
		_AddScalingModes(
			way != EffectAddedWay::Import,
			way == EffectAddedWay::Add);
	});
}

void ScalingModesViewModel::_ScalingModesService_Moved(uint32_t fromIndex, uint32_t toIndex) {
	if (_handlingUserReorder) {
		return;
	}

	_updatingScalingModes = true;
	IInspectable movedItem = _scalingModes.GetAt(fromIndex);
	_scalingModes.RemoveAt(fromIndex);
	_scalingModes.InsertAt(toIndex, movedItem);
	_updatingScalingModes = false;
}

void ScalingModesViewModel::_ScalingModesService_Removed(uint32_t index) {
	_updatingScalingModes = true;
	_scalingModes.RemoveAt(index);
	_updatingScalingModes = false;
	RaisePropertyChanged(L"CanReorderScalingModes");
}

void ScalingModesViewModel::_ScalingModesService_Reset() {
	++_collectionGeneration;
	_pendingInitialExpanded = false;
	_pendingAutoRename = false;
	_movingFromIdx = std::numeric_limits<uint32_t>::max();

	_updatingScalingModes = true;
	_scalingModes.Clear();
	_updatingScalingModes = false;
	_AddScalingModes();
	RaisePropertyChanged(L"CanReorderScalingModes");
}

void ScalingModesViewModel::_ScalingModes_VectorChanged(
	IObservableVector<IInspectable> const&,
	IVectorChangedEventArgs const& args) {
	if (_updatingScalingModes) {
		return;
	}

	if (args.CollectionChange() == CollectionChange::ItemRemoved) {
		_movingFromIdx = args.Index();
		return;
	}
	if (args.CollectionChange() != CollectionChange::ItemInserted ||
		_movingFromIdx == std::numeric_limits<uint32_t>::max()) {
		return;
	}

	const uint32_t movingToIdx = args.Index();
	const uint32_t movingFromIdx = std::exchange(
		_movingFromIdx,
		std::numeric_limits<uint32_t>::max());
	_handlingUserReorder = true;
	ScalingModesService::Get().MoveScalingMode(movingFromIdx, movingToIdx);
	_handlingUserReorder = false;
}

}
