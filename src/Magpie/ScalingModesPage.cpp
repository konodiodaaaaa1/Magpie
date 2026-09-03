#include "pch.h"
#include "ScalingModesPage.h"
#if __has_include("ScalingModesPage.g.cpp")
#include "ScalingModesPage.g.cpp"
#endif
#include "ControlHelper.h"
#include "ContentDialogHelper.h"
#include "EffectsService.h"
#include "App.h"
#include "CommonSharedConstants.h"
#include "Logger.h"
#include <cmath>
#include <parallel_hashmap/phmap.h>
#include <winrt/Windows.Devices.Input.h>
#include <winrt/Windows.UI.Input.h>

using namespace ::Magpie;
using namespace winrt;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Input;

namespace winrt::Magpie::implementation {

ScalingModesPage::ScalingModesPage() {
	_BuildEffectMenu();
}

void ScalingModesPage::ComboBox_DropDownOpened(IInspectable const& sender, IInspectable const&) {
	ControlHelper::ComboBox_DropDownOpened(sender);
}

void ScalingModesPage::NumberBox_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	ControlHelper::NumberBox_Loaded(sender);
}

void ScalingModesPage::EffectSettingsCard_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	XamlHelper::UpdateThemeOfTooltips(sender.try_as<DependencyObject>(), ActualTheme());
}

void ScalingModesPage::AddEffectButton_Click(IInspectable const& sender, RoutedEventArgs const&) {
	Button btn = sender.try_as<Button>();
	_curScalingMode = get_self<ScalingModeItem>(btn.Tag().try_as<winrt::Magpie::ScalingModeItem>());
	_addEffectMenuFlyout.ShowAt(btn);
}

void ScalingModesPage::NewScalingModeButton_Click(IInspectable const&, RoutedEventArgs const&) {
	_viewModel->AddScalingMode();
}

fire_and_forget ScalingModesPage::ResetScalingModesButton_Click(
	IInspectable const&,
	RoutedEventArgs const&) {
	if (ContentDialogHelper::IsAnyDialogOpen()) {
		co_return;
	}

	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	ContentDialog dialog;
	dialog.XamlRoot(XamlRoot());
	dialog.RequestedTheme(ActualTheme());
	dialog.Title(box_value(resourceLoader.GetString(L"ScalingModes_ResetDialog_Title")));
	dialog.Content(box_value(resourceLoader.GetString(L"ScalingModes_ResetDialog_Content")));
	dialog.PrimaryButtonText(resourceLoader.GetString(L"ScalingModes_ResetDialog_Confirm"));
	dialog.CloseButtonText(resourceLoader.GetString(L"ScalingModes_ResetDialog_Cancel"));
	dialog.DefaultButton(ContentDialogButton::Close);

	if (co_await ContentDialogHelper::ShowAsync(dialog) == ContentDialogResult::Primary) {
		ScalingModesService::Get().ResetScalingModes();
	}
}

void ScalingModesPage::ScalingModeRenameButton_Loaded(
	IInspectable const& sender,
	RoutedEventArgs const&) {
	Button button = sender.try_as<Button>();
	_QueueAutoRename(button, button.DataContext());
}

void ScalingModesPage::ScalingModeRenameButton_DataContextChanged(
	FrameworkElement const& sender,
	DataContextChangedEventArgs const& args) {
	_QueueAutoRename(sender.try_as<Button>(), args.NewValue());
}

void ScalingModesPage::_QueueAutoRename(
	Button const& button,
	IInspectable const& candidateItem) noexcept {
	if (!button || !button.IsLoaded()) {
		return;
	}

	winrt::Magpie::ScalingModeItem item =
		candidateItem.try_as<winrt::Magpie::ScalingModeItem>();
	if (!item) {
		return;
	}

	weak_ref<Button> weakButton(button);
	App::Get().Dispatcher().TryEnqueue(DispatcherQueuePriority::Low, [weakButton, item]() {
		Button currentButton = weakButton.get();
		if (!currentButton || !currentButton.IsLoaded()) {
			return;
		}

		winrt::Magpie::ScalingModeItem currentItem =
			currentButton.DataContext().try_as<winrt::Magpie::ScalingModeItem>();
		if (!currentItem || currentItem != item) {
			return;
		}

		ScalingModeItem* itemImpl = get_self<ScalingModeItem>(item);
		if (!itemImpl->TakeAutoRenameRequest()) {
			return;
		}

		try {
			currentButton.Flyout().ShowAt(currentButton);
		} catch (...) {
			// The item can be removed or recycled before the queued callback runs.
			// In that case, opening the rename flyout is best-effort.
		}
	});
}

void ScalingModesPage::RenameTextBox_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	TextBox textBox = sender.try_as<TextBox>();
	textBox.Focus(FocusState::Programmatic);
	textBox.SelectAll();
}

void ScalingModesPage::RemoveScalingModeButton_Click(IInspectable const& sender, RoutedEventArgs const&) {
	Button button = sender.try_as<Button>();
	ScalingModeItem* scalingModeItem = get_self<ScalingModeItem>(
		button.Tag().try_as<winrt::Magpie::ScalingModeItem>());
	if (scalingModeItem->IsInUse()) {
		// 如果有缩放配置正在使用此缩放模式则弹出确认弹窗
		FlyoutBase::GetAttachedFlyout(button).ShowAt(button);
	} else {
		scalingModeItem->Remove();
	}
}

ListView ScalingModesPage::_FindParentListView(DependencyObject const& element) const noexcept {
	DependencyObject current = VisualTreeHelper::GetParent(element);
	while (current) {
		if (ListView listView = current.try_as<ListView>()) {
			return listView;
		}
		current = VisualTreeHelper::GetParent(current);
	}

	return nullptr;
}

uint32_t ScalingModesPage::_GetReorderTargetIndex(double pointerY) const noexcept {
	if (!_reorderListView || !_reorderItems || !_reorderItem) {
		return 0;
	}

	if (_reorderOriginalIndex >= _reorderItems.Size() ||
		_reorderItemCenters.size() != _reorderItems.Size()) {
		return _reorderOriginalIndex;
	}

	try {
		uint32_t targetIndex = 0;
		bool foundContainer = false;
		const uint32_t size = _reorderItems.Size();
		for (uint32_t i = 0; i < size; ++i) {
			if (i == _reorderOriginalIndex) {
				continue;
			}

			const double centerY = _reorderItemCenters[i];
			if (!std::isfinite(centerY)) {
				continue;
			}

			foundContainer = true;
			if (pointerY < centerY) {
				break;
			}
			++targetIndex;
		}

		return foundContainer ? std::min(targetIndex, size - 1) : _reorderOriginalIndex;
	} catch (...) {
		return _reorderOriginalIndex;
	}
}

void ScalingModesPage::_PrepareReorderPreview() noexcept {
	_reorderPreviewItems.clear();
	_reorderItemCenters.clear();
	_reorderSlotExtent = 0;

	if (!_reorderListView || !_reorderItems || !_reorderContainer) {
		return;
	}

	try {
		const uint32_t size = _reorderItems.Size();
		_reorderPreviewItems.reserve(size > 0 ? size - 1 : 0);
		_reorderItemCenters.assign(size, std::numeric_limits<double>::quiet_NaN());

		double draggedTop = 0;
		double nextTop = 0;
		bool hasDraggedTop = false;
		bool hasNextTop = false;

		for (uint32_t i = 0; i < size; ++i) {
			FrameworkElement container =
				_reorderListView.ContainerFromIndex(i).try_as<FrameworkElement>();
			if (!container) {
				continue;
			}

			const Point topLeft =
				container.TransformToVisual(_reorderListView).TransformPoint({});
			_reorderItemCenters[i] = topLeft.Y + container.ActualHeight() / 2;

			if (i == _reorderOriginalIndex) {
				draggedTop = topLeft.Y;
				hasDraggedTop = true;
				continue;
			}
			if (i == _reorderOriginalIndex + 1) {
				nextTop = topLeft.Y;
				hasNextTop = true;
			}

			ReorderPreviewItem preview;
			preview.index = i;
			preview.container = container;
			preview.originalRenderTransform = container.RenderTransform();
			preview.previewTransform = CompositeTransform();

			TransformGroup transforms;
			if (preview.originalRenderTransform) {
				transforms.Children().Append(preview.originalRenderTransform);
			}
			transforms.Children().Append(preview.previewTransform);
			container.RenderTransform(transforms);
			_reorderPreviewItems.emplace_back(std::move(preview));
		}

		if (hasDraggedTop && hasNextTop && nextTop > draggedTop) {
			_reorderSlotExtent = static_cast<float>(nextTop - draggedTop);
		} else {
			const Thickness margin = _reorderContainer.Margin();
			_reorderSlotExtent = static_cast<float>(
				_reorderContainer.ActualHeight() + margin.Top + margin.Bottom);
		}
	} catch (...) {
		_ClearReorderPreview();
	}
}

void ScalingModesPage::_UpdateReorderPreview(uint32_t targetIndex) noexcept {
	if (_reorderSlotExtent <= 0) {
		return;
	}

	try {
		for (const ReorderPreviewItem& preview : _reorderPreviewItems) {
			double offset = 0;
			if (_reorderOriginalIndex < targetIndex &&
				preview.index > _reorderOriginalIndex && preview.index <= targetIndex) {
				offset = -_reorderSlotExtent;
			} else if (targetIndex < _reorderOriginalIndex &&
				preview.index >= targetIndex && preview.index < _reorderOriginalIndex) {
				offset = _reorderSlotExtent;
			}

			preview.previewTransform.TranslateY(offset);
		}
	} catch (...) {
		// Containers can be unrealized if the list scrolls during a drag. The next
		// pointer event will continue updating the remaining realized containers.
	}
}

void ScalingModesPage::_ClearReorderPreview() noexcept {
	for (const ReorderPreviewItem& preview : _reorderPreviewItems) {
		try {
			preview.container.RenderTransform(preview.originalRenderTransform);
		} catch (...) {
		}
	}

	_reorderPreviewItems.clear();
	_reorderItemCenters.clear();
	_reorderSlotExtent = 0;
}

void ScalingModesPage::_QueueFinishReorder(bool commit) noexcept {
	if (!_reorderHandle) {
		return;
	}

	// PointerCaptureLost is normally raised after PointerReleased. Once a valid
	// release has requested a commit, the later capture-lost notification must not
	// turn it into a cancellation.
	_queuedReorderCommit = _queuedReorderCommit || commit;
	if (_isReorderFinishQueued) {
		return;
	}

	_isReorderFinishQueued = true;
	const uint32_t pointerId = _reorderPointerId;
	try {
		weak_ref<ScalingModesPage> weakThis = get_weak();
		if (App::Get().Dispatcher().TryEnqueue(
			DispatcherQueuePriority::Low,
			[weakThis, pointerId]() {
				com_ptr<ScalingModesPage> self = weakThis.get();
				if (!self || !self->_isReorderFinishQueued ||
					self->_reorderPointerId != pointerId) {
					return;
				}

				const bool shouldCommit = std::exchange(self->_queuedReorderCommit, false);
				self->_isReorderFinishQueued = false;
				self->_FinishReorder(shouldCommit);
			})) {
			return;
		}
	} catch (...) {
	}

	// Dispatch only fails during shutdown. Do not touch XAML objects from the
	// current input callback; the page teardown will release the references.
	_isReorderFinishQueued = false;
	_queuedReorderCommit = false;
}

void ScalingModesPage::_FinishReorder(bool commit) noexcept {
	FrameworkElement container = std::exchange(_reorderContainer, nullptr);
	IObservableVector<IInspectable> items = std::exchange(_reorderItems, nullptr);
	IInspectable item = std::exchange(_reorderItem, nullptr);
	const uint32_t targetIndex = _reorderTargetIndex;
	const bool wasDragging = _isReorderDragging;

	if (commit && wasDragging && items && item) {
		try {
			uint32_t currentIndex = 0;
			if (items.IndexOf(item, currentIndex) && currentIndex != targetIndex) {
				// Keep the preview transforms until the reordered collection has completed
				// its layout. Clearing them first exposes the old slots for one rendered
				// frame, the same release-time twitch fixed in the profile list.
				items.RemoveAt(currentIndex);
				items.InsertAt(std::min(targetIndex, items.Size()), item);
				if (_reorderListView) {
					_reorderListView.UpdateLayout();
				}
			}
		} catch (...) {
			// The page may be closing while the pointer is captured. In that case the
			// collection or its item can already be detached, so cancel the reorder.
		}
	}

	_ClearReorderPreview();

	if (container) {
		try {
			container.RenderTransform(_originalRenderTransform);
			container.Opacity(_originalOpacity);
			Canvas::SetZIndex(container, _originalZIndex);
		} catch (...) {
			// The container may have been unrealized while completion was queued.
		}
	}

	_reorderHandle = nullptr;
	_reorderListView = nullptr;
	_originalRenderTransform = nullptr;
	_dragTransform = nullptr;
	_reorderPointerId = 0;
	_reorderOriginalIndex = 0;
	_isReorderDragging = false;
	_isReorderFinishQueued = false;
	_queuedReorderCommit = false;
}

void ScalingModesPage::ReorderHandle_PointerPressed(
	IInspectable const& sender,
	PointerRoutedEventArgs const& args) {
	try {
		FrameworkElement handle = sender.try_as<FrameworkElement>();
		if (_reorderHandle || !handle || !handle.Tag()) {
			return;
		}

		auto pointerPoint = args.GetCurrentPoint(handle);
		if (pointerPoint.PointerDevice().PointerDeviceType() ==
			Windows::Devices::Input::PointerDeviceType::Mouse &&
			!pointerPoint.Properties().IsLeftButtonPressed()) {
			return;
		}

		ListView listView = _FindParentListView(handle);
		if (!listView) {
			return;
		}

		IObservableVector<IInspectable> items =
			listView.ItemsSource().try_as<IObservableVector<IInspectable>>();
		uint32_t itemIndex = 0;
		if (!items || items.Size() < 2 || !items.IndexOf(handle.Tag(), itemIndex)) {
			return;
		}

		FrameworkElement container =
			listView.ContainerFromIndex(itemIndex).try_as<FrameworkElement>();
		if (!container || !handle.CapturePointer(args.Pointer())) {
			return;
		}

		_reorderHandle = handle;
		_reorderContainer = container;
		_reorderListView = listView;
		_reorderItems = items;
		_reorderItem = handle.Tag();
		_reorderOriginalIndex = itemIndex;
		_originalRenderTransform = container.RenderTransform();
		_originalOpacity = container.Opacity();
		_originalZIndex = Canvas::GetZIndex(container);
		_dragTransform = CompositeTransform();

		TransformGroup transforms;
		if (_originalRenderTransform) {
			transforms.Children().Append(_originalRenderTransform);
		}
		transforms.Children().Append(_dragTransform);
		container.RenderTransform(transforms);
		container.Opacity(0.92);
		Canvas::SetZIndex(container, 1000);
		_PrepareReorderPreview();

		_pointerStartY = args.GetCurrentPoint(listView).Position().Y;
		_reorderTargetIndex = itemIndex;
		_reorderPointerId = args.Pointer().PointerId();
		_isReorderDragging = false;
		args.Handled(true);
	} catch (const hresult_error& e) {
		Logger::Get().ComWarn("启动拖拽失败", e.code());
		_QueueFinishReorder(false);
	} catch (...) {
		Logger::Get().Warn("启动拖拽失败");
		_QueueFinishReorder(false);
	}
}

void ScalingModesPage::ReorderHandle_PointerMoved(
	IInspectable const& sender,
	PointerRoutedEventArgs const& args) {
	try {
		if (!_reorderHandle || sender != _reorderHandle ||
			args.Pointer().PointerId() != _reorderPointerId) {
			return;
		}

		const double pointerY = args.GetCurrentPoint(_reorderListView).Position().Y;
		const double deltaY = pointerY - _pointerStartY;
		if (!_isReorderDragging && std::abs(deltaY) >= 3) {
			_isReorderDragging = true;
		}

		if (_isReorderDragging) {
			_dragTransform.TranslateY(deltaY);
			const uint32_t targetIndex = _GetReorderTargetIndex(pointerY);
			if (targetIndex != _reorderTargetIndex) {
				_reorderTargetIndex = targetIndex;
				_UpdateReorderPreview(targetIndex);
			}
		}
		args.Handled(true);
	} catch (const hresult_error& e) {
		Logger::Get().ComWarn("更新拖拽位置失败", e.code());
		_QueueFinishReorder(false);
	} catch (...) {
		Logger::Get().Warn("更新拖拽位置失败");
		_QueueFinishReorder(false);
	}
}

void ScalingModesPage::ReorderHandle_PointerReleased(
	IInspectable const& sender,
	PointerRoutedEventArgs const& args) {
	try {
		if (!_reorderHandle || sender != _reorderHandle ||
			args.Pointer().PointerId() != _reorderPointerId) {
			return;
		}

		args.Handled(true);
		_QueueFinishReorder(true);
	} catch (const hresult_error& e) {
		Logger::Get().ComWarn("完成拖拽失败", e.code());
		_QueueFinishReorder(false);
	} catch (...) {
		Logger::Get().Warn("完成拖拽失败");
		_QueueFinishReorder(false);
	}
}

void ScalingModesPage::ReorderHandle_PointerCanceled(
	IInspectable const& sender,
	PointerRoutedEventArgs const& args) {
	try {
		if (!_reorderHandle || sender != _reorderHandle ||
			args.Pointer().PointerId() != _reorderPointerId) {
			return;
		}

		args.Handled(true);
		_QueueFinishReorder(false);
	} catch (const hresult_error& e) {
		Logger::Get().ComWarn("取消拖拽失败", e.code());
		_QueueFinishReorder(false);
	} catch (...) {
		Logger::Get().Warn("取消拖拽失败");
		_QueueFinishReorder(false);
	}
}

void ScalingModesPage::ReorderHandle_PointerCaptureLost(
	IInspectable const& sender,
	PointerRoutedEventArgs const& args) {
	try {
		if (_reorderHandle && sender == _reorderHandle &&
			args.Pointer().PointerId() == _reorderPointerId) {
			_QueueFinishReorder(false);
		}
	} catch (const hresult_error& e) {
		Logger::Get().ComWarn("处理拖拽捕获丢失失败", e.code());
		_QueueFinishReorder(false);
	} catch (...) {
		Logger::Get().Warn("处理拖拽捕获丢失失败");
		_QueueFinishReorder(false);
	}
}

void ScalingModesPage::_BuildEffectMenu() noexcept {
	std::vector<MenuFlyoutItemBase> rootItems;

	phmap::flat_hash_map<std::wstring_view, MenuFlyoutSubItem> folders;
	folders.reserve(13);
	for (const auto& effect : EffectsService::Get().Effects()) {
		std::wstring_view name(effect.name);

		MenuFlyoutItem item;
		item.Tag(box_value(effect.name));
		item.Click({ this, &ScalingModesPage::_AddEffectMenuFlyoutItem_Click });

		size_t delimPos = name.find_last_of(L'\\');
		if (delimPos == std::wstring::npos) {
			item.Text(name);
			rootItems.emplace_back(std::move(item));
			continue;
		}

		item.Text(name.substr(delimPos + 1));

		std::wstring_view dir = name.substr(0, delimPos);
		auto it = folders.find(dir);
		if (it != folders.end()) {
			it->second.Items().Append(item);
		} else {
			MenuFlyoutSubItem folder;
			folder.Text(hstring(dir));
			folder.Items().Append(item);

			rootItems.push_back(folder);
			folders.emplace(dir, folder);
		}
	}

	std::sort(rootItems.begin(), rootItems.end(), [](MenuFlyoutItemBase const& l, MenuFlyoutItemBase const& r) {
		bool isLSubMenu = get_class_name(l) == name_of<MenuFlyoutSubItem>();
		bool isRSubMenu = get_class_name(r) == name_of<MenuFlyoutSubItem>();

		if (isLSubMenu != isRSubMenu) {
			return isLSubMenu;
		}

		if (isLSubMenu) {
			return l.try_as<MenuFlyoutSubItem>().Text() < r.try_as<MenuFlyoutSubItem>().Text();
		} else {
			return l.try_as<MenuFlyoutItem>().Text() < r.try_as<MenuFlyoutItem>().Text();
		}
	});

	// 排序文件夹中的项目
	for (MenuFlyoutItemBase& item : rootItems) {
		MenuFlyoutSubItem folder = item.try_as<MenuFlyoutSubItem>();
		if (!folder) {
			break;
		}

		IVector<MenuFlyoutItemBase> items = folder.Items();
		// 读取到 std::vector 中以提高排序性能
		std::vector<MenuFlyoutItemBase> itemsVec(items.Size(), nullptr);
		items.GetMany(0, itemsVec);
		std::sort(itemsVec.begin(), itemsVec.end(), [](const MenuFlyoutItemBase& l, const MenuFlyoutItemBase& r) {
			hstring lEffectName = unbox_value<hstring>(l.try_as<MenuFlyoutItem>().Tag());
			hstring rEffectName = unbox_value<hstring>(r.try_as<MenuFlyoutItem>().Tag());

			const EffectInfo* lEffectInfo = EffectsService::Get().GetEffect(lEffectName);
			const EffectInfo* rEffectInfo = EffectsService::Get().GetEffect(rEffectName);

			return lEffectInfo->sortName < rEffectInfo->sortName;
		});
		items.ReplaceAll(itemsVec);
	}

	for (MenuFlyoutItemBase& item : rootItems) {
		_addEffectMenuFlyout.Items().Append(std::move(item));
	}
}

void ScalingModesPage::_AddEffectMenuFlyoutItem_Click(IInspectable const& sender, RoutedEventArgs const&) {
	hstring effectName = unbox_value<hstring>(sender.try_as<MenuFlyoutItem>().Tag());
	_curScalingMode->AddEffect(effectName);
}

}
