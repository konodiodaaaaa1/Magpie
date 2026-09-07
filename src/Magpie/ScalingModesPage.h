#pragma once
#include "ScalingModesPage.g.h"
#include "ScalingModeItem.h"
#include "ScalingModesViewModel.h"
#include "EffectParameterResetGesture.h"
#include <unordered_set>

namespace winrt::Magpie::implementation {

struct ScalingModesPage : ScalingModesPageT<ScalingModesPage> {
	ScalingModesPage();

	winrt::Magpie::ScalingModesViewModel ViewModel() const noexcept {
		return *_viewModel;
	}

	void ComboBox_DropDownOpened(IInspectable const& sender, IInspectable const&);

	void NumberBox_Loaded(IInspectable const& sender, RoutedEventArgs const&);
	void ParameterSlider_Loaded(IInspectable const& sender, RoutedEventArgs const&);
	void ParameterSlider_Unloaded(IInspectable const& sender, RoutedEventArgs const&);
	void ParameterSlider_LostFocus(IInspectable const& sender, RoutedEventArgs const&);
	void ParameterSlider_DataContextChanged(FrameworkElement const& sender,
		DataContextChangedEventArgs const&);
	void ParameterSlider_ValueChanged(IInspectable const& sender,
		Controls::Primitives::RangeBaseValueChangedEventArgs const&);

	void EffectSettingsCard_Loaded(IInspectable const& sender, RoutedEventArgs const&);

	void EffectParametersFlyout_Opening(IInspectable const& sender, IInspectable const&);

	void AddEffectButton_Click(IInspectable const& sender, RoutedEventArgs const&);

	void NewScalingModeButton_Click(IInspectable const& sender, RoutedEventArgs const&);

	fire_and_forget ResetScalingModesButton_Click(
		IInspectable const& sender,
		RoutedEventArgs const& args);

	void ScalingModeRenameButton_Loaded(IInspectable const& sender, RoutedEventArgs const&);

	void ScalingModeRenameButton_DataContextChanged(
		FrameworkElement const& sender,
		DataContextChangedEventArgs const& args);

	void RenameTextBox_Loaded(IInspectable const& sender, RoutedEventArgs const&);

	void RemoveScalingModeButton_Click(IInspectable const& sender, RoutedEventArgs const&);

	void ReorderHandle_PointerPressed(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ReorderHandle_PointerMoved(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ReorderHandle_PointerReleased(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ReorderHandle_PointerCanceled(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ReorderHandle_PointerCaptureLost(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);
private:
	void _ParameterSlider_Pressed(IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);
	void _ParameterSlider_Moved(IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);
	void _ParameterSlider_Released(IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);
	void _ParameterSlider_Canceled(IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);
	void _ParameterSlider_CaptureLost(IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);
	void _RefreshParameterSliderHint(Slider const& slider);
	::Magpie::EffectParameterResetGesture _parameterResetGesture;
	IInspectable _parameterSliderPressed{ nullptr };
	IInspectable _parameterSliderMoved{ nullptr };
	IInspectable _parameterSliderReleased{ nullptr };
	IInspectable _parameterSliderCanceled{ nullptr };
	IInspectable _parameterSliderCaptureLost{ nullptr };
	std::unordered_set<uintptr_t> _parameterSliders;
	weak_ref<Slider> _resetHeldSlider;
	bool _capturingResetPointer = false;

	struct ReorderPreviewItem {
		uint32_t index = 0;
		FrameworkElement container{ nullptr };
		Transform originalRenderTransform{ nullptr };
		CompositeTransform previewTransform{ nullptr };
	};

	void _BuildEffectMenu() noexcept;
	void _RefreshEffectMenuAvailability() noexcept;

	void _AddEffectMenuFlyoutItem_Click(IInspectable const& sender, RoutedEventArgs const&);

	ListView _FindParentListView(DependencyObject const& element) const noexcept;

	uint32_t _GetReorderTargetIndex(double pointerY) const noexcept;

	void _PrepareReorderPreview() noexcept;

	void _UpdateReorderPreview(uint32_t targetIndex) noexcept;

	void _ClearReorderPreview() noexcept;

	void _QueueFinishReorder(bool commit) noexcept;

	void _FinishReorder(bool commit) noexcept;

	void _QueueAutoRename(
		Button const& button,
		IInspectable const& candidateItem) noexcept;

	MenuFlyout _addEffectMenuFlyout;
	com_ptr<ScalingModesViewModel> _viewModel = make_self<ScalingModesViewModel>();
	ScalingModeItem* _curScalingMode = nullptr;

	FrameworkElement _reorderHandle{ nullptr };
	FrameworkElement _reorderContainer{ nullptr };
	ListView _reorderListView{ nullptr };
	IObservableVector<IInspectable> _reorderItems{ nullptr };
	IInspectable _reorderItem{ nullptr };
	Transform _originalRenderTransform{ nullptr };
	CompositeTransform _dragTransform{ nullptr };
	std::vector<ReorderPreviewItem> _reorderPreviewItems;
	std::vector<double> _reorderItemCenters;
	double _originalOpacity = 1;
	double _pointerStartY = 0;
	float _reorderSlotExtent = 0;
	int32_t _originalZIndex = 0;
	uint32_t _reorderOriginalIndex = 0;
	uint32_t _reorderTargetIndex = 0;
	uint32_t _reorderPointerId = 0;
	bool _isReorderDragging = false;
	bool _isReorderFinishQueued = false;
	bool _queuedReorderCommit = false;
};

}

BASIC_FACTORY(ScalingModesPage)
