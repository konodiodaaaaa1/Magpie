#include "pch.h"
#include "HdrDiagnostics.h"
#include "HdrProtocol.h"
#include "Logger.h"

namespace Magpie {

void LogHdrDiagnostics(const HdrDiagnostics& diagnostics, bool debugOnly) noexcept {
    const bool hasIssue = !diagnostics.fallbackReason.empty() || !diagnostics.assumptions.empty();
    (void)debugOnly;

    const std::string message = fmt::format(
        "HDR diagnostics\n"
        "\thdrOptionEnabled: {}\n"
        "\tcaptureMethod: {}\n"
        "\tsourceFormat: {}\n"
        "\tsourceColorDescriptionValid: {}\n"
        "\tsourceDxgiColorSpace: {}\n"
        "\tsourcePrimaries: {}\n"
        "\tsourceTransfer: {}\n"
        "\tsourceRange: {}\n"
        "\tsourceReferenceWhiteNits: {:.3f}\n"
        "\tsourceSdrWhiteNits: {:.3f}\n"
        "\tsourceDisplayPeakNits: {:.3f}\n"
        "\tdisplayHdrEnabled: {}\n"
        "\tsourceSceneReferred: {}\n"
        "\tsourceColorInferred: {}\n"
        "\tsourcePreExposure: {:.6f}\n"
        "\tcanonicalFormat: {}\n"
        "\tselectedAdapterProfile: {}\n"
        "\tselectedRouteId: {}\n"
        "\tconversionPath: {}\n"
        "\tfallbackReason: {}\n"
        "\tassumptions: {}",
        diagnostics.hdrOptionEnabled ? "true" : "false",
        diagnostics.captureMethod.empty() ? "(not set)" : diagnostics.captureMethod,
        static_cast<uint32_t>(diagnostics.sourceFormat),
        diagnostics.sourceColorDescription.IsValid() ? "true" : "false",
        static_cast<uint32_t>(diagnostics.sourceColorDescription.dxgiColorSpace),
        static_cast<uint32_t>(diagnostics.sourceColorDescription.primaries),
        ToString(diagnostics.sourceColorDescription.transfer),
        ToString(diagnostics.sourceColorDescription.range),
        diagnostics.sourceColorDescription.referenceWhiteNits,
        diagnostics.sourceColorDescription.sdrWhiteNits,
        diagnostics.sourceColorDescription.displayPeakNits,
        diagnostics.sourceColorDescription.displayHdrEnabled ? "true" : "false",
        diagnostics.sourceColorDescription.isSceneReferred ? "true" : "false",
        diagnostics.sourceColorDescription.isInferred ? "true" : "false",
        diagnostics.sourceColorDescription.preExposure,
        static_cast<uint32_t>(diagnostics.canonicalFormat),
        ToString(diagnostics.selectedAdapterProfile),
        diagnostics.selectedRouteId.empty() ? "(not set)" : diagnostics.selectedRouteId,
        diagnostics.conversionPath.empty() ? "(not set)" : diagnostics.conversionPath,
        diagnostics.fallbackReason.empty() ? "(none)" : diagnostics.fallbackReason,
        diagnostics.assumptions.empty() ? "(none)" : diagnostics.assumptions
    );

    if (hasIssue) {
        Logger::Get().Warn(message);
    } else {
        Logger::Get().Info(message);
    }
}

void AppendHdrAssumption(HdrDiagnostics& diagnostics, std::string_view assumption) noexcept {
    if (assumption.empty()) {
        return;
    }

    if (diagnostics.assumptions.empty()) {
        diagnostics.assumptions = std::string(assumption);
    } else {
        diagnostics.assumptions.append("; ");
        diagnostics.assumptions.append(assumption);
    }
}

}
