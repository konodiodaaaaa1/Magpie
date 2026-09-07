#include "pch.h"
#include "HdrEffectBoundary.h"

namespace Magpie {

namespace {

HdrFormatRoute MakeGenericSdrFallbackRoute() {
	return HdrFormatRoute{
		.effectId = "__generic__",
		.optionId = "sdr-compatible-fallback",
		.inputFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
		.outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
		.inputTransfer = HdrTransferFunction::SRGB,
		.outputTransfer = HdrTransferFunction::SRGB,
		.inputRange = HdrColorRange::Full,
		.outputRange = HdrColorRange::Full,
		.alphaMode = HdrAlphaMode::Preserve,
		.evidenceLevel = HdrEvidenceLevel::None,
		.hdrNative = false,
		.adapterProfile = HdrAdapterProfile::SDRCompatible,
	};
}

}

const HdrFormatRoute* HdrEffectBoundary::SelectRoute(bool hdrEnabled, const HdrFormatRoutes& routes) noexcept {
	return hdrEnabled ? SelectDefaultHdrRoute(routes) : SelectDefaultSdrRoute(routes);
}

HdrEffectBoundaryContext HdrEffectBoundary::Prepare(bool hdrEnabled, const HdrFrame& inputFrame, const HdrFormatRoutes& routes, const ColorDescription& sourceColor) noexcept {
	HdrEffectBoundaryContext context;
	context.hdrEnabled = hdrEnabled;
	context.inputFrame = inputFrame;
	context.routes = routes;
	const HdrFormatRoute* selectedRoute = SelectRoute(hdrEnabled, context.routes);
	if (hdrEnabled && !selectedRoute) {
		context.routes.push_back(MakeGenericSdrFallbackRoute());
		selectedRoute = &context.routes.back();
	}
	if (selectedRoute) {
		context.selectedRouteIndex = static_cast<size_t>(selectedRoute - context.routes.data());
	}

	if (!hdrEnabled) {
		context.prepared = true;
		return context;
	}

	if (!context.inputFrame.IsCanonical() || !context.SelectedRoute()) {
		return context;
	}

	context.plan = HdrAdapterDispatcher{}.BuildPlan(*context.SelectedRoute(), sourceColor);
	context.prepared = context.plan.IsNonTerminalCanonical() || context.plan.isPresentationTerminal;
	return context;
}

bool HdrEffectBoundary::Complete(HdrEffectBoundaryContext& context, const HdrFrame& backendOutput) noexcept {
	if (!context.prepared) {
		return false;
	}
	if (!context.hdrEnabled || context.plan.isPresentationTerminal) {
		context.completed = true;
		return true;
	}
	if (!backendOutput.IsCanonical()) {
		return false;
	}
	context.completed = true;
	return true;
}

}
