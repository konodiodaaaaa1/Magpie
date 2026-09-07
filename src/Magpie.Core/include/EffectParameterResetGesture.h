#pragma once
#include <cmath>
#include <cstdint>

namespace Magpie {

// Use input event timestamps (microseconds), not the time the UI draws a frame.
// Each pair is consumed once; a drag, another control or a cancellation breaks it.
class EffectParameterResetGesture {
public:
	static constexpr uint64_t THRESHOLD_US = 170000;

	bool Press(uint64_t control, uint64_t timeUs,
		float x, float y, float tolerance) noexcept {
		const bool reset = control != 0 && control == _control && _released &&
			timeUs > _timeUs && timeUs - _timeUs <= THRESHOLD_US &&
			std::abs(x - _x) <= tolerance && std::abs(y - _y) <= tolerance;
		Clear();
		if (!reset) {
			_control = control;
			_timeUs = timeUs;
			_x = x;
			_y = y;
			_down = true;
		}
		return reset;
	}

	void Move(float x, float y, float tolerance) noexcept {
		if (_down && (std::abs(x - _x) > tolerance ||
			std::abs(y - _y) > tolerance)) Clear();
	}

	void Release(bool dragged = false) noexcept {
		if (dragged) Clear();
		else if (_control && _down) {
			_down = false;
			_released = true;
		}
	}

	void Clear() noexcept {
		_control = 0;
		_down = false;
		_released = false;
	}

private:
	uint64_t _control = 0;
	uint64_t _timeUs = 0;
	float _x = 0.0f;
	float _y = 0.0f;
	bool _down = false;
	bool _released = false;
};

}
