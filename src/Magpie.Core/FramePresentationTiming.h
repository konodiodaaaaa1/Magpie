#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>

namespace Magpie {

// One deadline per submitted image/input. Small lateness can correct phase,
// but never admits a new frame less than 75% of a period after the previous
// one. Long stalls re-anchor instead of accumulating catch-up submissions.
class FrontEdgeSyncClock {
public:
	using Clock = std::chrono::steady_clock;
	void SetInterval(std::chrono::nanoseconds interval) noexcept {
		interval = std::max(interval, std::chrono::nanoseconds(1));
		if (_interval != interval) { _interval = interval; Reset(); }
	}
	Clock::time_point Due(Clock::time_point now) const noexcept {
		return _started ? _next : now;
	}
	void Submitted(Clock::time_point now) noexcept {
		const auto due = Due(now);
		_next = now > due + _interval ? now + _interval :
			std::max(due + _interval, now + _interval * 3 / 4);
		_started = true;
	}
	void Reset() noexcept { _started = false; _next = {}; }
private:
	std::chrono::nanoseconds _interval{16'666'667};
	Clock::time_point _next{};
	bool _started = false;
};

// Limit repeats of the last content image, not delivery of new content. Every
// successful shared-layer content frame also carries the current overlay.
class OverlayPresentationClock {
public:
	using Clock = std::chrono::steady_clock;
	void SetRefreshRate(double refreshRate) noexcept {
		if (!(refreshRate >= 10.0 && refreshRate <= 1000.0)) refreshRate = 60.0;
		_interval = std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::duration<double>(1.0 / refreshRate));
	}
	bool IsDue(Clock::time_point now, bool immediate) const noexcept {
		return immediate || !_started || now - _last >= _interval;
	}
	void Presented(Clock::time_point now) noexcept { _last = now; _started = true; }
	std::chrono::nanoseconds PollInterval() const noexcept {
		return std::min(_interval, std::chrono::nanoseconds(std::chrono::milliseconds(8)));
	}
	std::chrono::nanoseconds Interval() const noexcept { return _interval; }
private:
	Clock::time_point _last{};
	std::chrono::nanoseconds _interval{ 16'666'666 };
	bool _started = false;
};

// Estimate pacing without feeding downstream queue waits back into the next
// interval. Those waits are consequences of presentation, not source cadence.
class CaptureFrameCadence {
public:
	using Clock = std::chrono::steady_clock;
	bool Observe(Clock::time_point now,
		Clock::duration downstreamWait = Clock::duration::zero()) noexcept {
		const auto elapsed = now - _last;
		_lastDownstreamWait = std::clamp(downstreamWait, Clock::duration::zero(),
			std::max(elapsed, Clock::duration::zero()));
		const bool interrupted = _started && elapsed >= std::chrono::milliseconds(500);
		const auto sourceElapsed = elapsed - _lastDownstreamWait;
		if (_started && !interrupted && sourceElapsed > Clock::duration::zero()) {
			const double seconds = std::chrono::duration<double>(sourceElapsed).count();
			_seconds = _hasSequenceEstimate ? _seconds + (seconds - _seconds) * 0.2 : seconds;
			_hasSequenceEstimate = true;
		} else if (interrupted) {
			// Keep the last healthy interval as a fallback, but do not blend the
			// first new pair with the previous sequence's samples.
			_hasSequenceEstimate = false;
		}
		_started = true;
		_last = now;
		return interrupted;
	}
	Clock::duration LastDownstreamWait() const noexcept { return _lastDownstreamWait; }
	std::chrono::nanoseconds Interval(uint32_t multiplier, double baseLimit) const noexcept {
		const double minimum = baseLimit > 0 ? 1.0 / baseLimit : 0.0;
		const double seconds = std::max(_seconds > 0 ? _seconds : 1.0 / 60.0, minimum);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::duration<double>(seconds / std::max(multiplier, 1u)));
	}
	void Reset() noexcept { *this = {}; }
	void RestartSequence() noexcept {
		_started = false;
		_hasSequenceEstimate = false;
		_lastDownstreamWait = {};
	}
private:
	Clock::time_point _last{};
	Clock::duration _lastDownstreamWait{};
	double _seconds = 0;
	bool _started = false;
	bool _hasSequenceEstimate = false;
};

// No catch-up burst after a long stall. The frontend polls this deadline while
// continuing to service input; capacity waits remain owned by the presenter.
class FramePresentationClock {
public:
	using Clock = std::chrono::steady_clock;
	Clock::time_point Due(Clock::time_point now, std::chrono::nanoseconds interval) const noexcept {
		return _started ? _last + interval : now;
	}
	void Presented(Clock::time_point now, Clock::time_point due,
		std::chrono::nanoseconds interval) noexcept {
		_last = interval.count() > 0 && now <= due + interval ? due : now;
		_started = true;
	}
	void Reset() noexcept { *this = {}; }
private:
	Clock::time_point _last{};
	bool _started = false;
};

}
