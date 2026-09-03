#pragma once
#include "EffectParametersViewModel.g.h"
#include "ScalingModeParameter.g.h"
#include <parallel_hashmap/phmap.h>

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
		float step
	) : _index(index), _label(std::move(label)), _minimum(minimum),
		_maximum(maximum), _step(step), _value(initValue) {}

	uint32_t Index() const noexcept {
		return _index;
	}

	bool IsBoolean() const noexcept {
		return _isBoolean;
	}

	bool IsFloat() const noexcept {
		return !_isBoolean;
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

	double Value() const noexcept {
		return _value;
	}

	void Value(double value) {
		_value = value;
		RaisePropertyChanged(L"Value");
		RaisePropertyChanged(L"ValueText");
	}

	hstring ValueText() const noexcept;

	hstring Label() const noexcept {
		return _label;
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
	bool _isVisible = true;
	const double _minimum = 0.0;
	const double _maximum = 1.0;
	const double _step = 1.0;
	double _value;
};

struct EffectParametersViewModel : EffectParametersViewModelT<EffectParametersViewModel> {
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

	IVector<IInspectable> Params() const noexcept {
		return _params;
	}

private:
	bool _IsRemoved() const noexcept;

	void _ScalingModeParameter_PropertyChanged(
		IInspectable const& sender,
		PropertyChangedEventArgs const& args
	);

	phmap::flat_hash_map<std::wstring, float>& _Data() const;

	IVector<IInspectable> _params{ nullptr };
	com_ptr<ScalingModeParameter> _inputResolutionPercent;
	com_ptr<ScalingModeParameter> _residualMultiplier;

	uint32_t _scalingModeIdx;
	uint32_t _effectIdx;
	const ::Magpie::EffectInfo* _effectInfo = nullptr;
};

}
