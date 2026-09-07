#pragma once

#include "HdrFrame.h"

#include <string>
#include <string_view>

namespace Magpie {

// Compact, CPU-side diagnostic state. It is intended for DEBUG or existing
// logger output at capture/effect boundaries. It never triggers a CPU staging
// readback and does not alter rendering synchronization.
struct HdrDiagnostics {
    bool hdrOptionEnabled = false;
    std::string captureMethod;
    DXGI_FORMAT sourceFormat = DXGI_FORMAT_UNKNOWN;
    ColorDescription sourceColorDescription{};
    DXGI_FORMAT canonicalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    HdrAdapterProfile selectedAdapterProfile = HdrAdapterProfile::Unknown;
    std::string selectedRouteId;
    std::string conversionPath;
    std::string fallbackReason;
    std::string assumptions;
};

// Emits the structured diagnostic through the existing logger. debugOnly is a
// policy hint for callers; implementations may still log warnings for explicit
// fallback/assumption reasons.
void LogHdrDiagnostics(const HdrDiagnostics& diagnostics, bool debugOnly = false) noexcept;

// Convenience for populating "missing source color metadata" assumptions.
void AppendHdrAssumption(HdrDiagnostics& diagnostics, std::string_view assumption) noexcept;

}
