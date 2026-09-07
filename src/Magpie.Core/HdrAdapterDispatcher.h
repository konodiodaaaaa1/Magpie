#pragma once

#include "HdrColorTransform.h"
#include "HdrFrame.h"
#include "HdrProtocol.h"

#include <string>

namespace Magpie {

// Reusable plan produced by HdrAdapterDispatcher. This task deliberately does
// not attach the dispatcher to a concrete effect backend; the plan describes
// the conversion boundary so a future Renderer/effect integration can execute
// it without duplicating profile/route rules.
struct HdrAdapterPlan {
    static constexpr DXGI_FORMAT CanonicalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    HdrAdapterProfile profile = HdrAdapterProfile::Unknown;
    std::string routeId;
    std::string conversionPath;
    DXGI_FORMAT canonicalInputFormat = CanonicalFormat;
    // Non-terminal routes always return to the canonical FP16 surface.
    // PresentationTerminal leaves the ordinary canonical texture chain.
    DXGI_FORMAT canonicalOutputFormat = CanonicalFormat;
    HdrAlphaMode alphaMode = HdrAlphaMode::Unknown;
    bool isPresentationTerminal = false;
    bool requiresSdrMapping = false;
    bool requiresBoundedMapping = false;
    bool usesFallback = false;
    std::string fallbackReason;
    float normalizationScale = 1.0f;
    // SDRCompatible and BoundedHDR use explicit paired forward/inverse state.
    HdrTransformParameters forwardParameters;
    HdrTransformParameters inverseParameters;

    bool IsNonTerminalCanonical() const noexcept {
        return !isPresentationTerminal &&
            canonicalInputFormat == CanonicalFormat &&
            canonicalOutputFormat == CanonicalFormat;
    }
};

// Selects conversion behavior from the structured route. No effect-name string
// is consulted and no concrete backend is invoked.
class HdrAdapterDispatcher {
public:
    HdrAdapterPlan BuildPlan(const HdrFormatRoute& route, const ColorDescription& sourceColor) noexcept;
};

}
