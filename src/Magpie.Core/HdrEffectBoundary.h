#pragma once

#include "HdrAdapterDispatcher.h"

#include <cstddef>

namespace Magpie {

struct HdrEffectBoundaryContext {
	bool hdrEnabled = false;
	HdrFrame inputFrame{};
	HdrFormatRoutes routes{};
	size_t selectedRouteIndex = static_cast<size_t>(-1);
	HdrAdapterPlan plan{};
	bool prepared = false;
	bool completed = false;

	bool IsCanonicalInput() const noexcept {
		return !hdrEnabled || inputFrame.IsCanonical();
	}

	const HdrFormatRoute* SelectedRoute() const noexcept {
		return selectedRouteIndex < routes.size() ? &routes[selectedRouteIndex] : nullptr;
	}
};

class HdrEffectBoundary {
public:
	static HdrEffectBoundaryContext Prepare(bool hdrEnabled, const HdrFrame& inputFrame, const HdrFormatRoutes& routes, const ColorDescription& sourceColor) noexcept;
	static bool Complete(HdrEffectBoundaryContext& context, const HdrFrame& backendOutput) noexcept;
	static const HdrFormatRoute* SelectRoute(bool hdrEnabled, const HdrFormatRoutes& routes) noexcept;
};

}
