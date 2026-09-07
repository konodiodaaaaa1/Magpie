#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

namespace Magpie {

enum class EffectParametersSaveError : uint8_t {
	None, WriteFailed, Conflict, SessionExpired, SourceUnavailable
};

// Owned by requests as well as the overlay: completions never reference a
// destroyed window. An older write cannot overwrite a newer result.
struct EffectParametersSaveState {
	std::atomic<uint64_t> result = 0;

	void Complete(uint64_t revision, EffectParametersSaveError error) noexcept {
		uint64_t previous = result.load(std::memory_order_acquire);
		const uint64_t next = (revision << 3) | static_cast<uint64_t>(error);
		while ((previous >> 3) <= revision &&
			!result.compare_exchange_weak(previous, next,
				std::memory_order_release, std::memory_order_acquire)) {}
	}
};

// Three-way parameter merge. Preserve unrelated editor changes and reject a
// concurrent edit to the same parameter. The caller commits the full chain
// only after every effect has passed validation.
template<typename Map>
bool MergeEffectParameterChanges(Map& current, const Map& before, const Map& after) {
	for (const auto& [name, value] : after) {
		if (!std::isfinite(value)) return false;
		const auto old = before.find(name);
		if (old != before.end() && old->second == value) continue;
		const auto existing = current.find(name);
		if (existing != current.end() && existing->second == value) continue;
		if ((existing == current.end()) != (old == before.end()) ||
			(existing != current.end() && existing->second != old->second)) {
			return false;
		}
		current[name] = value;
	}
	return true;
}

}
