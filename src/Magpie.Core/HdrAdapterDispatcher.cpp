#include "pch.h"
#include "HdrAdapterDispatcher.h"

namespace Magpie {

namespace {

constexpr std::string_view PathForProfile(HdrAdapterProfile profile) noexcept {
    switch (profile) {
    case HdrAdapterProfile::DirectFP16:
        return "canonicalFP16->DirectFP16->canonicalFP16";
    case HdrAdapterProfile::BoundedHDR:
        return "canonicalFP16->boundedHdrEncode->backend->boundedHdrDecode->canonicalFP16";
    case HdrAdapterProfile::SDRCompatible:
        return "canonicalFP16->HdrToSdr->backend->SdrToHdr->canonicalFP16";
    case HdrAdapterProfile::ConditionalFP16:
        return "canonicalFP16->ConditionalFP16->canonicalFP16";
    case HdrAdapterProfile::PresentationTerminal:
        return "canonicalFP16->presentationEncoder->presentationTerminal";
    case HdrAdapterProfile::Unknown:
    default:
        return "canonicalFP16->SdrCompatibleFallback->backend->SdrToHdr->canonicalFP16";
    }
}

HdrTransformParameters ParametersForColor(const ColorDescription& color) noexcept {
    const HdrTransformParameters params = HdrColorTransform::ForFrame(color);
    return params.IsValid() ? params : HdrTransformParameters{};
}

}

HdrAdapterPlan HdrAdapterDispatcher::BuildPlan(
    const HdrFormatRoute& route,
    const ColorDescription& sourceColor
) noexcept {
    HdrAdapterPlan plan;
    plan.routeId = route.Id();
    plan.profile = route.adapterProfile;
    plan.alphaMode = route.alphaMode;
    plan.forwardParameters = ParametersForColor(sourceColor);
    plan.inverseParameters = ParametersForColor(sourceColor);
    plan.normalizationScale = route.normalizationScale > 0.0f ? route.normalizationScale : 1.0f;

    switch (route.adapterProfile) {
    case HdrAdapterProfile::DirectFP16:
        plan.conversionPath = PathForProfile(HdrAdapterProfile::DirectFP16);
        plan.requiresSdrMapping = false;
        plan.requiresBoundedMapping = false;
        break;

    case HdrAdapterProfile::BoundedHDR:
        plan.conversionPath = PathForProfile(HdrAdapterProfile::BoundedHDR);
        plan.requiresBoundedMapping = true;
        break;

    case HdrAdapterProfile::SDRCompatible:
        plan.conversionPath = PathForProfile(HdrAdapterProfile::SDRCompatible);
        plan.requiresSdrMapping = true;
        break;

    case HdrAdapterProfile::ConditionalFP16:
        plan.conversionPath = PathForProfile(HdrAdapterProfile::ConditionalFP16);
        plan.requiresSdrMapping = false;
        plan.requiresBoundedMapping = false;
        break;

    case HdrAdapterProfile::PresentationTerminal:
        plan.conversionPath = PathForProfile(HdrAdapterProfile::PresentationTerminal);
        plan.canonicalOutputFormat = DXGI_FORMAT_UNKNOWN;
        plan.isPresentationTerminal = true;
        break;

    case HdrAdapterProfile::Unknown:
    default:
        plan.conversionPath = PathForProfile(HdrAdapterProfile::Unknown);
        plan.requiresSdrMapping = true;
        plan.usesFallback = true;
        plan.fallbackReason = "No verified HDR route; using the existing compatible SDR fallback.";
        if (plan.alphaMode == HdrAlphaMode::Unknown) {
            plan.alphaMode = HdrAlphaMode::ForceOpaque;
        }
        break;
    }

    if (plan.alphaMode == HdrAlphaMode::Unknown) {
        plan.alphaMode = HdrAlphaMode::ForceOpaque;
        plan.fallbackReason = plan.fallbackReason.empty()
            ? "Route did not declare an alpha rule; using ForceOpaque."
            : plan.fallbackReason + " Route did not declare an alpha rule; using ForceOpaque.";
    }

    return plan;
}

}
