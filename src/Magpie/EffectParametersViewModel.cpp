#include "pch.h"
#include "EffectParametersViewModel.h"
#if __has_include("ScalingModeParameter.g.cpp")
#include "ScalingModeParameter.g.cpp"
#endif
#if __has_include("EffectParametersViewModel.g.cpp")
#include "EffectParametersViewModel.g.cpp"
#endif
#include "StrHelper.h"
#include "AppSettings.h"
#include "ScalingModesService.h"
#include "ScalingMode.h"
#include "EffectsService.h"
#include "EffectDesc.h"
#include "App.h"

using namespace Magpie;

namespace winrt::Magpie::implementation {

// 限制保存频率
// 1 秒内没有新的调用才执行保存
static fire_and_forget LazySaveAppSettings() {
	using namespace std::chrono;

	static steady_clock::time_point lastInvokeTime;
	static bool sleeping = false;

	lastInvokeTime = steady_clock::now();

	if (sleeping) {
		co_return;
	}

	sleeping = true;
	
	co_await 1s;

	while (true) {
		int duration = (int)duration_cast<milliseconds>(steady_clock::now() - lastInvokeTime).count();
		if (duration >= 999) {
			break;
		}

		// 如果与 lastInvokeTime 相差不到 1s，则继续等待
		co_await milliseconds(1000 - duration);
	}

	// 回到主线程
	co_await App::Get().Dispatcher();

	sleeping = false;
	AppSettings::Get().SaveAsync();
}

EffectParametersViewModel::EffectParametersViewModel(uint32_t scalingModeIdx, uint32_t effectIdx)
	: _scalingModeIdx(scalingModeIdx), _effectIdx(effectIdx)
{
	ScalingMode& scalingMode = ScalingModesService::Get().GetScalingMode(_scalingModeIdx);
	_effectInfo = EffectsService::Get().GetEffect(scalingMode.effects[_effectIdx].name);
	const bool isDlssnr = scalingMode.effects[_effectIdx].name ==
		L"DLSSNR\\DLSSNR_AI_Filter";

	phmap::flat_hash_map<std::wstring, float>& params = _Data();

	std::vector<IInspectable> parameterItems;
	ScalingModeParameter* inputResolutionToggle = nullptr;
	for (uint32_t i = 0, size = (uint32_t)_effectInfo->params.size(); i < size; ++i) {
		const EffectParameterDesc& param = _effectInfo->params[i];

		std::optional<float> paramValue;
		{
			auto it = params.find(StrHelper::UTF8ToUTF16(param.name));
			if (it != params.end()) {
				paramValue = it->second;
			}
		}
		
		if (param.constant.index() == 0) {
			const EffectConstant<float>& constant = std::get<0>(param.constant);
			auto paramItem = make_self<ScalingModeParameter>(
				i,
				hstring(StrHelper::UTF8ToUTF16(param.label.empty() ? param.name : param.label)),
				paramValue.has_value() ? *paramValue : constant.defaultValue,
				constant.minValue,
				constant.maxValue,
				constant.step
			);
			paramItem->PropertyChanged({
				this, &EffectParametersViewModel::_ScalingModeParameter_PropertyChanged });
			if (isDlssnr && param.name == "residualMultiplier") {
				_residualMultiplier = paramItem;
			}
			parameterItems.push_back(*paramItem);
		} else {
			const EffectConstant<int>& constant = std::get<1>(param.constant);
			if (constant.minValue == 0 && constant.maxValue == 1 && constant.step == 1) {
				auto paramItem = make_self<ScalingModeParameter>(
					i,
					hstring(StrHelper::UTF8ToUTF16(param.label.empty() ? param.name : param.label)),
					paramValue.has_value() ? std::abs(*paramValue) > FLOAT_EPSILON<float> : (bool)constant.defaultValue
				);
				paramItem->PropertyChanged({
					this, &EffectParametersViewModel::_ScalingModeParameter_PropertyChanged });
				if (isDlssnr && param.name == "enableInputResolutionScaling") {
					inputResolutionToggle = paramItem.get();
				}
				parameterItems.push_back(*paramItem);
			} else {
				auto paramItem = make_self<ScalingModeParameter>(
					i,
					hstring(StrHelper::UTF8ToUTF16(param.label.empty() ? param.name : param.label)),
					paramValue.has_value() ? *paramValue : (float)constant.defaultValue,
					(float)constant.minValue,
					(float)constant.maxValue,
					(float)constant.step
				);
				paramItem->PropertyChanged({
					this, &EffectParametersViewModel::_ScalingModeParameter_PropertyChanged });
				if (isDlssnr && param.name == "inputResolutionPercent") {
					_inputResolutionPercent = paramItem;
				}
				parameterItems.push_back(*paramItem);
			}
		}
	}
	if (_inputResolutionPercent) {
		_inputResolutionPercent->IsVisible(
			inputResolutionToggle && inputResolutionToggle->BooleanValue());
	}
	if (_residualMultiplier) {
		_residualMultiplier->IsVisible(
			inputResolutionToggle && inputResolutionToggle->BooleanValue());
	}
	if (!isDlssnr) {
		std::stable_partition(
			parameterItems.begin(), parameterItems.end(),
			[](const IInspectable& item) {
				return get_self<ScalingModeParameter>(
					item.as<Magpie::ScalingModeParameter>())->IsBoolean();
			});
	}
	if (!parameterItems.empty()) {
		_params = single_threaded_vector(std::move(parameterItems));
	}
}

// 应确保被删除后依然处于合法的状态，调用任何方法都不会崩溃，见 ScalingModeItem::_IsRemoved
bool EffectParametersViewModel::_IsRemoved() const noexcept {
	return _scalingModeIdx == std::numeric_limits<uint32_t>::max() ||
		_effectIdx == std::numeric_limits<uint32_t>::max();
}

void EffectParametersViewModel::_ScalingModeParameter_PropertyChanged(
	IInspectable const& sender,
	PropertyChangedEventArgs const& args
) {
	if (_IsRemoved() ||
		(args.PropertyName() != L"Value" &&
		 args.PropertyName() != L"BooleanValue")) {
		return;
	}

	ScalingModeParameter* paramImpl = get_self<ScalingModeParameter>(
		sender.try_as<Magpie::ScalingModeParameter>());
	const std::string& effectName = _effectInfo->params[paramImpl->Index()].name;
	_Data()[StrHelper::UTF8ToUTF16(effectName)] = paramImpl->IsBoolean() ?
		static_cast<float>(paramImpl->BooleanValue()) :
		static_cast<float>(paramImpl->Value());
	if (effectName == "enableInputResolutionScaling" &&
		_inputResolutionPercent) {
		_inputResolutionPercent->IsVisible(paramImpl->BooleanValue());
		if (_residualMultiplier) {
			_residualMultiplier->IsVisible(paramImpl->BooleanValue());
		}
	}

	LazySaveAppSettings();
}

phmap::flat_hash_map<std::wstring, float>& EffectParametersViewModel::_Data() const {
	ScalingMode& scalingMode = ScalingModesService::Get().GetScalingMode(_scalingModeIdx);
	return scalingMode.effects[_effectIdx].parameters;
}

hstring ScalingModeParameter::ValueText() const noexcept {
	return App::DoubleFormatter().FormatDouble(_value);
}

}
