#pragma once
#include "FramePacingOptions.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Magpie {

// Called only for parameters otherwise supported by the running live backend.
inline bool NeedsDlssNrParameterRestart(std::string_view effect,
	std::string_view parameter, bool hasLaterDlssNr) noexcept {
	if (IsFrameRateFilterEffect(effect)) return false;
	if (hasLaterDlssNr) return true;
	if (effect != "DLSSNR\\DLSSNR_AI_Filter") return false;
	return parameter != "residualMultiplier" && parameter != "residualSaturation" &&
		parameter != "residualLightness" && parameter != "shadowStructureMultiplier" &&
		parameter != "reflectionGlowMultiplier";
}

class EffectParameterRestartQueue {
public:
	using Clock = std::chrono::steady_clock;
	struct Change { uint32_t effect; std::string parameter; float value; };
	bool Update(uint32_t effect, const std::string& parameter, float value,
		float applied, Clock::time_point now) {
		if (_waiting || !std::isfinite(value)) return false;
		auto it = std::find_if(_changes.begin(), _changes.end(), [&](const Change& c) {
			return c.effect == effect && c.parameter == parameter;
		});
		if (std::abs(value - applied) <= 1e-6f) {
			if (it != _changes.end()) _changes.erase(it);
		} else if (it == _changes.end()) {
			_changes.push_back({ effect, parameter, value });
		} else {
			it->value = value;
		}
		_due = now + std::chrono::milliseconds(300);
		return true;
	}
	bool ReadyToStop(Clock::time_point now, bool inputActive) const noexcept {
		return !_waiting && !_changes.empty() && !inputActive && now >= _due;
	}
	std::vector<Change> TakeChanges() { return std::exchange(_changes, {}); }
	void WaitAfterStop(Clock::time_point now) noexcept {
		_waiting = true;
		_due = now + std::chrono::milliseconds(500);
	}
	bool IsWaiting() const noexcept { return _waiting; }
	bool HasChanges() const noexcept { return !_changes.empty(); }
	bool ReadyToStart(Clock::time_point now) const noexcept { return _waiting && now >= _due; }
	void Cancel() noexcept { _changes.clear(); _waiting = false; }
private:
	std::vector<Change> _changes;
	Clock::time_point _due{};
	bool _waiting = false;
};

}
