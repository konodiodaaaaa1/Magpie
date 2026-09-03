#pragma once
#include "ProfilePage.g.h"
#include "ProfileViewModel.h"

namespace winrt::Magpie::implementation {

struct ProfilePage : ProfilePageT<ProfilePage> {
	void OnNavigatedTo(Navigation::NavigationEventArgs const& args);

	winrt::Magpie::ProfileViewModel ViewModel() const noexcept {
		return *_viewModel;
	}

	void ComboBox_DropDownOpened(IInspectable const& sender, IInspectable const&);

	void NumberBox_Loaded(IInspectable const& sender, RoutedEventArgs const&);

	void InitialWindowedScaleFactorComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&);

	void CursorScalingComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&);

	void LaunchParametersTextBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args);

private:
	com_ptr<ProfileViewModel> _viewModel;
};

}

BASIC_FACTORY(ProfilePage)
