#include "pch.h"
#include "ProfilePage.h"
#if __has_include("ProfilePage.g.cpp")
#include "ProfilePage.g.cpp"
#endif
#include "App.h"
#include "ControlHelper.h"
#include "Profile.h"

using namespace ::Magpie;
using namespace winrt;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Input;

namespace winrt::Magpie::implementation {

void ProfilePage::OnNavigatedTo(Navigation::NavigationEventArgs const& args) {
	int profileIdx = args.Parameter().try_as<int>().value();
	_viewModel = make_self<ProfileViewModel>(profileIdx);
}

void ProfilePage::ComboBox_DropDownOpened(IInspectable const& sender, IInspectable const&) {
	ControlHelper::ComboBox_DropDownOpened(sender);
}

void ProfilePage::NumberBox_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	ControlHelper::NumberBox_Loaded(sender);
}

void ProfilePage::InitialWindowedScaleFactorComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&) {
	if (InitialWindowedScaleFactorFromSelectedIndex(_viewModel->InitialWindowedScaleFactor()) ==
		InitialWindowedScaleFactor::Custom)
	{
		InitialWindowedScaleFactorComboBox().MinWidth(0);
		CustomInitialWindowedScaleFactorNumberBox().Visibility(Visibility::Visible);
		CustomInitialWindowedScaleFactorLabel().Visibility(Visibility::Visible);
	} else {
		const double minWidth = App::Get().Resources()
			.Lookup(box_value(L"SettingsCardContentMinWidth"))
			.try_as<double>().value();
		InitialWindowedScaleFactorComboBox().MinWidth(minWidth);
		CustomInitialWindowedScaleFactorNumberBox().Visibility(Visibility::Collapsed);
		CustomInitialWindowedScaleFactorLabel().Visibility(Visibility::Collapsed);
	}
}

void ProfilePage::CursorScalingComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&) {
	if ((CursorScaling)_viewModel->CursorScaling() == CursorScaling::Custom) {
		CursorScalingComboBox().MinWidth(0);
		CustomCursorScalingNumberBox().Visibility(Visibility::Visible);
		CustomCursorScalingLabel().Visibility(Visibility::Visible);
	} else {
		const double minWidth = App::Get().Resources()
			.Lookup(box_value(L"SettingsCardContentMinWidth"))
			.try_as<double>().value();
		CursorScalingComboBox().MinWidth(minWidth);
		CustomCursorScalingNumberBox().Visibility(Visibility::Collapsed);
		CustomCursorScalingLabel().Visibility(Visibility::Collapsed);
	}
}

void ProfilePage::LaunchParametersTextBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args) {
	if (args.Key() == VirtualKey::Enter) {
		Focus(FocusState::Pointer);
	}
}

}
