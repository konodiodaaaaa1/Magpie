#pragma once
#include "EffectParameterGroupViewModel.g.h"
#include "EffectParametersViewModel.g.h"
#include "ScalingModeParameter.g.h"
#include <parallel_hashmap/phmap.h>
#include "Event.h"

namespace Magpie {
struct EffectInfo;
}

namespace winrt::Magpie::implementation {

struct ScalingModeParameter : ScalingModeParameterT<ScalingModeParameter>,
		wil::notify_property_changed_base<ScalingModeParameter> {
	ScalingModeParameter(uint32_t index, hstring label, bool initValue)
		: _index(index), _label(std::move(label)), _isBoolean(true),
		_value(initValue ? 1.0 : 0.0) {}

	ScalingModeParameter(
		uint32_t index,
		hstring label,
		float initValue,
		float minimum,
		float maximum,
		float step,
		float defaultValue
	) : _index(index), _label(std::move(label)), _minimum(minimum),
		_maximum(maximum), _step(step), _defaultValue(defaultValue), _value(initValue) {}

	ScalingModeParameter(
		uint32_t index,
		hstring label,
		int initValue,
		std::vector<std::pair<int, hstring>> choices
	) : _index(index), _label(std::move(label)), _isChoice(true),
		_value(initValue) {
		_RebuildChoices(choices);
		assert(ChoiceIndex() >= 0 &&
			static_cast<uint32_t>(ChoiceIndex()) < _choices.Size());
	}

	void _RebuildChoices(
		const std::vector<std::pair<int, hstring>>& choices
	) {
		std::vector<hstring> items;
		items.reserve(choices.size());
		_choiceValues.clear();
		_choiceValues.reserve(choices.size());
		for (const auto& [value, choiceLabel] : choices) {
			_choiceValues.push_back(value);
			items.push_back(choiceLabel);
		}
		if (_choices) {
			_choices.ReplaceAll(items);
		} else {
			_choices = single_threaded_observable_vector(std::move(items));
		}
		assert(_choices.Size() == choices.size());
		assert(_choices.try_as<IIterable<IInspectable>>());
	}

	uint32_t Index() const noexcept {
		return _index;
	}

	bool IsBoolean() const noexcept {
		return _isBoolean;
	}

	bool IsChoice() const noexcept {
		return _isChoice;
	}

	bool IsFloat() const noexcept {
		return !_isBoolean && !_isChoice;
	}

	bool IsVisible() const noexcept {
		return _isVisible;
	}

	void IsVisible(bool value) {
		if (_isVisible == value) return;
		_isVisible = value;
		RaisePropertyChanged(L"IsVisible");
	}

	bool BooleanValue() const noexcept {
		return _value != 0.0;
	}

	void BooleanValue(bool value) {
		_value = value ? 1.0 : 0.0;
		RaisePropertyChanged(L"BooleanValue");
	}

	bool IsEnabled() const noexcept { return _isEnabled; }
	void IsEnabled(bool value) {
		if (_isEnabled == value) return;
		_isEnabled = value;
		RaisePropertyChanged(L"IsEnabled");
	}

	int32_t ChoiceValue() const noexcept {
		return static_cast<int32_t>(std::lround(_value));
	}

	void ChoiceValue(int32_t value) {
		if (!_isEnabled || std::find(_choiceValues.begin(), _choiceValues.end(), value) ==
			_choiceValues.end() || ChoiceValue() == value) {
			return;
		}
		_value = value;
		RaisePropertyChanged(L"ChoiceValue");
		RaisePropertyChanged(L"ChoiceIndex");
	}

	int32_t ChoiceIndex() const noexcept {
		const auto it = std::find(
			_choiceValues.begin(), _choiceValues.end(), ChoiceValue());
		return it == _choiceValues.end() ? -1 :
			static_cast<int32_t>(std::distance(_choiceValues.begin(), it));
	}

	void ChoiceIndex(int32_t index) {
		if (index < 0 || static_cast<size_t>(index) >= _choiceValues.size()) {
			return;
		}
		ChoiceValue(_choiceValues[index]);
	}

	IObservableVector<hstring> Choices() const noexcept {
		return _choices;
	}

	double Value() const noexcept {
		return _value;
	}

	void Value(double value) {
		if (!_isEnabled || _value == value) return;
		_value = value;
		RaisePropertyChanged(L"Value");
		RaisePropertyChanged(L"ValueText");
	}

	double DefaultValue() const noexcept { return _defaultValue; }
	void ResetToDefault() {
		if (IsFloat() && _isEnabled && _isVisible) Value(_defaultValue);
	}

	void SynchronizeValue(float value) {
		if (_value == value) return;
		_value = value;
		if (_isBoolean) RaisePropertyChanged(L"BooleanValue");
		else if (_isChoice) {
			RaisePropertyChanged(L"ChoiceValue");
			RaisePropertyChanged(L"ChoiceIndex");
		} else {
			RaisePropertyChanged(L"Value");
			RaisePropertyChanged(L"ValueText");
		}
	}

	hstring ValueText() const noexcept;

	hstring Label() const noexcept {
		return _label;
	}

	bool HasDescriptionLine() const noexcept {
		return std::wstring_view(_label).find(L'\n') != std::wstring_view::npos;
	}

	bool HasNoDescriptionLine() const noexcept {
		return !HasDescriptionLine();
	}

	hstring PrimaryLabel() const {
		const std::wstring_view label(_label);
		const size_t lineBreak = label.find(L'\n');
		return lineBreak == std::wstring_view::npos ?
			_label : hstring(label.substr(0, lineBreak));
	}

	hstring DescriptionLabel() const {
		const std::wstring_view label(_label);
		const size_t lineBreak = label.find(L'\n');
		return lineBreak == std::wstring_view::npos ?
			hstring() : hstring(label.substr(lineBreak + 1));
	}

	double Minimum() const noexcept {
		return _minimum;
	}

	double Maximum() const noexcept {
		return _maximum;
	}

	double Step() const noexcept {
		return _step;
	}

private:
	const uint32_t _index;
	const hstring _label;
	const bool _isBoolean = false;
	const bool _isChoice = false;
	bool _isVisible = true;
	bool _isEnabled = true;
	const double _minimum = 0.0;
	const double _maximum = 1.0;
	const double _step = 1.0;
	const double _defaultValue = 0.0;
	double _value;
	IObservableVector<hstring> _choices{ nullptr };
	std::vector<int> _choiceValues;
};

struct EffectParameterGroupViewModel :
		EffectParameterGroupViewModelT<EffectParameterGroupViewModel>,
		wil::notify_property_changed_base<EffectParameterGroupViewModel> {
	EffectParameterGroupViewModel(
		hstring label,
		std::vector<IInspectable>&& params
	);

	hstring Label() const noexcept { return _label; }
	bool HasLabel() const noexcept { return !_label.empty(); }
	bool IsVisible() const noexcept { return _isVisible; }
	double ColumnWidth() const noexcept { return _columnWidth; }
	bool ShowLeadingSeparator() const noexcept { return _showLeadingSeparator; }
	IVector<IInspectable> Params() const noexcept { return _params; }
	void RefreshVisibility();
	void ColumnWidth(double value);
	void ShowLeadingSeparator(bool value);

private:
	hstring _label;
	IVector<IInspectable> _params{ nullptr };
	bool _isVisible = true;
	bool _showLeadingSeparator = false;
	double _columnWidth = 260.0;
};

struct EffectParametersViewModel : EffectParametersViewModelT<EffectParametersViewModel>,
		wil::notify_property_changed_base<EffectParametersViewModel> {
	EffectParametersViewModel(uint32_t scalingModeIdx, uint32_t effectIdx);

	uint32_t ScalingModeIdx() const noexcept {
		return _scalingModeIdx;
	}

	void ScalingModeIdx(uint32_t value) noexcept {
		_scalingModeIdx = value;
	}

	uint32_t EffectIdx() const noexcept {
		return _effectIdx;
	}

	void EffectIdx(uint32_t value) noexcept {
		_effectIdx = value;
	}

	IVector<IInspectable> Groups() const noexcept {
		return _groups;
	}
	double ContentWidth() const noexcept { return _contentWidth; }

	void UpdateLayoutWidth(double availableWidth);

private:
	bool _IsRemoved() const noexcept;

	void _ScalingModeParameter_PropertyChanged(
		IInspectable const& sender,
		PropertyChangedEventArgs const& args
	);

	phmap::flat_hash_map<std::wstring, float>& _Data() const;
	void _RefreshLayoutWidth();
	void _RefreshConditionalVisibility();

	IVector<IInspectable> _groups{ nullptr };
	std::vector<com_ptr<EffectParameterGroupViewModel>> _groupImpls;
	std::vector<com_ptr<ScalingModeParameter>> _parameterImpls;
	::Magpie::Event<uint32_t, uint32_t>::EventRevoker _parameterChangedRevoker;
	::Magpie::Event<>::EventRevoker _frontEdgeSyncChangedRevoker;
	bool _synchronizing = false;
	void _SynchronizeParameters();

	double _availableLayoutWidth = std::numeric_limits<double>::infinity();
	double _contentWidth = 260.0;

	uint32_t _scalingModeIdx;
	uint32_t _effectIdx;
	const ::Magpie::EffectInfo* _effectInfo = nullptr;
};

}
