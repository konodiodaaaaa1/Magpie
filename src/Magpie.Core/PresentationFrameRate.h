#pragma once
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Magpie {

struct PresentationRateSnapshot {
	uint32_t total = 0;
	uint32_t real = 0;
	bool totalKnown = true;
};

// Frontend-thread counter. Count accepted content frames, never overlay redraws
// or configured multipliers. An unavailable SDK count stays unknown.
class PresentationFrameRate {
public:
	using Clock = std::chrono::steady_clock;
	void Record(std::optional<uint32_t> total, uint32_t real,
		Clock::time_point now = Clock::now()) noexcept {
		_Update(now);
		if (total) _total += *total;
		else _known = false;
		_real += real;
	}
	PresentationRateSnapshot Get(Clock::time_point now = Clock::now()) noexcept {
		_Update(now);
		return _snapshot;
	}
	void Reset() noexcept { *this = {}; }

private:
	void _Update(Clock::time_point now) noexcept {
		if (!_started) { _start = now; _started = true; return; }
		const double elapsed = std::chrono::duration<double>(now - _start).count();
		if (elapsed < 1.0) return;
		_snapshot = { uint32_t(std::llround(_total / elapsed)),
			uint32_t(std::llround(_real / elapsed)), _known };
		_start = now;
		_total = _real = 0;
		_known = true;
	}
	Clock::time_point _start{};
	uint64_t _total = 0, _real = 0;
	bool _started = false, _known = true;
	PresentationRateSnapshot _snapshot{};
};

}
