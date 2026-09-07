#pragma once

#include "HdrFrame.h"

#include <string>
#include <string_view>
#include <vector>

namespace Magpie {

using HdrFormatRoutes = std::vector<HdrFormatRoute>;

// Textual names are for diagnostics and logging only. Protocol dispatch must
// use the structured route fields, never effect-name string comparisons.
constexpr std::string_view ToString(HdrAdapterProfile profile) noexcept {
    switch (profile) {
    case HdrAdapterProfile::DirectFP16:
        return "DirectFP16";
    case HdrAdapterProfile::BoundedHDR:
        return "BoundedHDR";
    case HdrAdapterProfile::SDRCompatible:
        return "SDRCompatible";
    case HdrAdapterProfile::ConditionalFP16:
        return "ConditionalFP16";
    case HdrAdapterProfile::Unknown:
        return "Unknown";
    case HdrAdapterProfile::PresentationTerminal:
        return "PresentationTerminal";
    default:
        return "Unknown";
    }
}

constexpr std::string_view ToString(HdrColorRange range) noexcept {
    switch (range) {
    case HdrColorRange::Full:
        return "Full";
    case HdrColorRange::Limited:
        return "Limited";
    case HdrColorRange::SceneLinear:
        return "SceneLinear";
    case HdrColorRange::DisplayReferred:
        return "DisplayReferred";
    case HdrColorRange::Unknown:
    default:
        return "Unknown";
    }
}

constexpr std::string_view ToString(HdrAlphaMode alphaMode) noexcept {
    switch (alphaMode) {
    case HdrAlphaMode::Preserve:
        return "Preserve";
    case HdrAlphaMode::ForceOpaque:
        return "ForceOpaque";
    case HdrAlphaMode::Premultiplied:
        return "Premultiplied";
    case HdrAlphaMode::Unknown:
    default:
        return "Unknown";
    }
}

constexpr std::string_view ToString(HdrEvidenceLevel evidenceLevel) noexcept {
    switch (evidenceLevel) {
    case HdrEvidenceLevel::PublicApiContract:
        return "PublicApiContract";
    case HdrEvidenceLevel::ReferenceImplementation:
        return "ReferenceImplementation";
    case HdrEvidenceLevel::CommunityExperiment:
        return "CommunityExperiment";
    case HdrEvidenceLevel::LocalValidation:
        return "LocalValidation";
    case HdrEvidenceLevel::None:
    default:
        return "None";
    }
}

constexpr std::string_view ToString(HdrTransferFunction transfer) noexcept {
    switch (transfer) {
    case HdrTransferFunction::Linear:
        return "Linear";
    case HdrTransferFunction::SRGB:
        return "SRGB";
    case HdrTransferFunction::PQ:
        return "PQ";
    case HdrTransferFunction::HLG:
        return "HLG";
    case HdrTransferFunction::Unknown:
    default:
        return "Unknown";
    }
}

// SDR mode returns the route marked defaultForSdr. HDR mode returns a route
// marked defaultForHdr, then falls back to an explicit HDR-native or adapter
// route. Unknown-profile routes are not selected because they cannot be
// considered accepted HDR routes.
const HdrFormatRoute* SelectDefaultHdrRoute(const HdrFormatRoutes& routes) noexcept;
const HdrFormatRoute* SelectDefaultSdrRoute(const HdrFormatRoutes& routes) noexcept;

std::vector<const HdrFormatRoute*> GetAcceptedFormatRoutes(const HdrFormatRoutes& routes);
std::vector<const HdrFormatRoute*> GetHdrNativeFormatRoutes(const HdrFormatRoutes& routes);
std::vector<const HdrFormatRoute*> GetHdrAdapterFormatRoutes(const HdrFormatRoutes& routes);

// Lightweight stable textual representation for diagnostics and future
// persistence tooling. It intentionally serializes every route field so the
// structured shape remains inspectable without effect-name branches.
std::string SerializeHdrFormatRoute(const HdrFormatRoute& route);

}
