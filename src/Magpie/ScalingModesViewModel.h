#pragma once
#include "ScalingModesViewModel.g.h"
#include "Event.h"
#include "ScalingModesService.h"

namespace winrt::Magpie::implementation {

struct ScalingModesViewModel : ScalingModesViewModelT<ScalingModesViewModel>,
                               wil::notify_property_changed_base<ScalingModesViewModel> {
	ScalingModesViewModel();

	fire_and_forget Export() noexcept;

	fire_and_forget Import();

	IObservableVector<IInspectable> ScalingModes() const noexcept {
		return _scalingModes;
	}

	bool CanReorderScalingModes() const noexcept;

	void AddScalingMode();

private:
	fire_and_forget _AddScalingModes(
		bool isInitialExpanded = false,
		bool shouldAutoRename = false);

	void _ScalingModesService_Added(::Magpie::EffectAddedWay way);

	void _ScalingModesService_Moved(uint32_t fromIndex, uint32_t toIndex);

	void _ScalingModesService_Removed(uint32_t index);

	void _ScalingModesService_Reset();

	void _ScalingModes_VectorChanged(
		IObservableVector<IInspectable> const&,
		IVectorChangedEventArgs const& args);

	IObservableVector<IInspectable> _scalingModes = single_threaded_observable_vector<IInspectable>();

	::Magpie::Event<::Magpie::EffectAddedWay>::EventRevoker _scalingModeAddedRevoker;
	::Magpie::Event<uint32_t, uint32_t>::EventRevoker _scalingModeMovedRevoker;
	::Magpie::Event<uint32_t>::EventRevoker _scalingModeRemovedRevoker;
	::Magpie::Event<>::EventRevoker _scalingModesResetRevoker;
	IObservableVector<IInspectable>::VectorChanged_revoker _scalingModesChangedRevoker;

	bool _addingScalingModes = false;
	bool _pendingInitialExpanded = false;
	bool _pendingAutoRename = false;
	bool _updatingScalingModes = false;
	bool _handlingUserReorder = false;
	uint32_t _movingFromIdx = std::numeric_limits<uint32_t>::max();
	uint32_t _collectionGeneration = 0;
};

}
