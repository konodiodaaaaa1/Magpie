#pragma once
#include "EffectDesc.h"

namespace Magpie {

inline bool IsChoiceEffectParameter(
	const EffectParameterDesc& parameter
) noexcept {
	return parameter.constant.index() == 1 && !parameter.choices.empty();
}

inline bool IsBooleanEffectParameter(const EffectParameterDesc& parameter) noexcept {
	if (parameter.constant.index() != 1 || IsChoiceEffectParameter(parameter)) {
		return false;
	}
	const EffectConstant<int>& constant = std::get<1>(parameter.constant);
	return constant.minValue == 0 && constant.maxValue == 1 && constant.step == 1;
}

inline float NormalizeEffectParameterValue(
	const EffectParameterDesc& parameter,
	float value
) noexcept {
	if (IsChoiceEffectParameter(parameter)) {
		const int defaultValue = std::get<1>(parameter.constant).defaultValue;
		if (!std::isfinite(value)) {
			return static_cast<float>(defaultValue);
		}
		const int candidate = static_cast<int>(std::llround(value));
		const auto it = std::ranges::find(
			parameter.choices, candidate, &EffectParameterChoice::value);
		return static_cast<float>(it == parameter.choices.end()
			? defaultValue : it->value);
	}
	if (parameter.constant.index() == 0) {
		const EffectConstant<float>& constant = std::get<0>(parameter.constant);
		if (!std::isfinite(constant.defaultValue) ||
			!std::isfinite(constant.minValue) ||
			!std::isfinite(constant.maxValue) ||
			constant.minValue > constant.maxValue) {
			return 0.0f;
		}
		if (!std::isfinite(value)) {
			value = constant.defaultValue;
		}
		value = std::clamp(value, constant.minValue, constant.maxValue);
		if (std::isfinite(constant.step) && constant.step > 0.0f) {
			const double minValue = constant.minValue;
			const double maxValue = constant.maxValue;
			const double step = constant.step;
			const double regularValue = std::clamp(
				minValue + std::round((double(value) - minValue) / step) * step,
				minValue, maxValue);
			// Treat MAX as a legal endpoint even when a third-party effect declares
			// a range that is not an exact multiple of STEP.
			value = static_cast<float>(
				std::abs(maxValue - value) < std::abs(regularValue - value)
					? maxValue : regularValue);
		}
		return std::clamp(value, constant.minValue, constant.maxValue);
	}

	const EffectConstant<int>& constant = std::get<1>(parameter.constant);
	if (constant.minValue > constant.maxValue) {
		return 0.0f;
	}
	const double finiteValue = std::isfinite(value)
		? std::clamp<double>(value, constant.minValue, constant.maxValue)
		: constant.defaultValue;
	int intValue = static_cast<int>(std::llround(finiteValue));
	intValue = std::clamp(intValue, constant.minValue, constant.maxValue);
	if (constant.step > 0) {
		const int64_t regularValue = std::clamp<int64_t>(
			int64_t(constant.minValue) + static_cast<int64_t>(std::llround(
				double(int64_t(intValue) - constant.minValue) / constant.step)) * constant.step,
			constant.minValue, constant.maxValue);
		if (std::abs(int64_t(constant.maxValue) - intValue) <
			std::abs(regularValue - intValue)) {
			intValue = constant.maxValue;
		} else {
			intValue = static_cast<int>(regularValue);
		}
	}
	return static_cast<float>(std::clamp(
		intValue, constant.minValue, constant.maxValue));
}

inline bool GetEffectParameterTicks(
	const EffectParameterDesc& parameter,
	float value,
	int& currentTick,
	int& maximumTick
) noexcept {
	if (IsChoiceEffectParameter(parameter)) {
		return false;
	}
	double minValue;
	double maxValue;
	double step;
	if (parameter.constant.index() == 0) {
		const EffectConstant<float>& constant = std::get<0>(parameter.constant);
		minValue = constant.minValue;
		maxValue = constant.maxValue;
		step = constant.step;
		if (!std::isfinite(minValue) || !std::isfinite(maxValue) ||
			!std::isfinite(step)) {
			return false;
		}
	} else {
		const EffectConstant<int>& constant = std::get<1>(parameter.constant);
		minValue = constant.minValue;
		maxValue = constant.maxValue;
		step = constant.step;
	}
	if (minValue > maxValue || step <= 0.0) {
		return false;
	}

	const double rawTickCount = (maxValue - minValue) / step;
	if (!std::isfinite(rawTickCount) ||
		rawTickCount > std::numeric_limits<int>::max() - 1.0) {
		return false;
	}
	const double roundedTickCount = std::round(rawTickCount);
	const double tickCount = std::abs(rawTickCount - roundedTickCount) <=
		std::max(1e-7, std::abs(rawTickCount) * 1e-7)
			? roundedTickCount : std::ceil(rawTickCount);
	maximumTick = std::max(1, static_cast<int>(tickCount));
	const double normalized = NormalizeEffectParameterValue(parameter, value);
	if (std::abs(normalized - maxValue) <=
		std::max(1e-7, std::abs(maxValue) * 1e-7)) {
		currentTick = maximumTick;
	} else {
		currentTick = std::clamp(
			static_cast<int>(std::lround((normalized - minValue) / step)),
			0, maximumTick);
	}
	return true;
}

inline float GetEffectParameterValueFromTick(
	const EffectParameterDesc& parameter,
	int tick,
	int maximumTick
) noexcept {
	tick = std::clamp(tick, 0, maximumTick);
	if (parameter.constant.index() == 0) {
		const EffectConstant<float>& constant = std::get<0>(parameter.constant);
		const double value = tick == maximumTick
			? constant.maxValue
			: double(constant.minValue) + double(tick) * constant.step;
		return NormalizeEffectParameterValue(parameter, static_cast<float>(value));
	}
	const EffectConstant<int>& constant = std::get<1>(parameter.constant);
	const int64_t value = tick == maximumTick
		? constant.maxValue
		: int64_t(constant.minValue) + int64_t(tick) * constant.step;
	return NormalizeEffectParameterValue(parameter, static_cast<float>(std::clamp<int64_t>(
		value, constant.minValue, constant.maxValue)));
}

inline int GetEffectParameterDisplayPrecision(
	const EffectParameterDesc& parameter
) noexcept {
	if (parameter.constant.index() != 0) {
		return 0;
	}
	const double step = std::get<0>(parameter.constant).step;
	if (!std::isfinite(step) || step <= 0.0) {
		return 3;
	}
	double scaled = step;
	for (int precision = 0; precision <= 6; ++precision) {
		if (std::abs(scaled - std::round(scaled)) <=
			std::max(1e-7, std::abs(scaled) * 1e-6)) {
			return precision;
		}
		scaled *= 10.0;
	}
	return 6;
}

}
