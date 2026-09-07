#include "pch.h"
#include "GroupAHdrRoutes.h"

namespace Magpie {

namespace {

HdrFormatRoute MakeSdrFallback(
    std::string_view effect,
    std::string_view option,
    DXGI_FORMAT backendFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
    HdrAlphaMode alpha = HdrAlphaMode::ForceOpaque
) {
    return HdrFormatRoute{
        .effectId = std::string(effect),
        .optionId = std::string(option),
        .inputFormat = backendFormat,
        .outputFormat = backendFormat,
        .inputTransfer = HdrTransferFunction::SRGB,
        .outputTransfer = HdrTransferFunction::SRGB,
        .inputRange = HdrColorRange::Full,
        .outputRange = HdrColorRange::Full,
        .alphaMode = alpha,
        .evidenceLevel = HdrEvidenceLevel::None,
        .hdrNative = false,
        .adapterProfile = HdrAdapterProfile::SDRCompatible,
        .defaultForHdr = true,
        .defaultForSdr = true,
    };
}

HdrFormatRoute MakeCasFp16() {
    return HdrFormatRoute{
        .effectId = "CAS",
        .optionId = "fp16-conditional",
        .inputFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .outputFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .inputTransfer = HdrTransferFunction::Linear,
        .outputTransfer = HdrTransferFunction::Linear,
        .inputRange = HdrColorRange::SceneLinear,
        .outputRange = HdrColorRange::SceneLinear,
        .alphaMode = HdrAlphaMode::ForceOpaque,
        .evidenceLevel = HdrEvidenceLevel::ReferenceImplementation,
        .hdrNative = false,
        .adapterProfile = HdrAdapterProfile::ConditionalFP16,
        .defaultForHdr = true,
        .defaultForSdr = false,
    };
}

bool IsGroupA(std::string_view effect) noexcept {
    return effect == "Anime4K" || effect == "CAS" || effect == "CRT" ||
        effect == "CuNNy" || effect == "CuNNy2" || effect == "Diagnostics" ||
        effect == "FSRCNNX" || effect == "FXAA" || effect == "MLAA";
}

}

GroupAHdrEffectDescription GetGroupAHdrEffectDescription(
	std::string_view effectName,
	int casFormatOption
) {
	if (effectName == "CAS") {
		HdrFormatRoutes routes;
		// The production CAS CSO is compiled from the declared R8 texture
		// contract. Keep HDR on the paired SDR adapter until a separately
		// compiled FP16 CAS variant exists; changing the runtime surface alone
		// would bind a route-incompatible UAV/SRV pair.
		routes.push_back(MakeSdrFallback("CAS", "r8-sdr", DXGI_FORMAT_R8G8B8A8_UNORM));
		return {
			.routes = std::move(routes),
			.auxiliaryResources = "none; RGB input/output, alpha forced opaque",
			.evidence = casFormatOption == 1
				? "CAS FP16 request held behind a separately compiled variant; using declared R8 path"
				: "Current Magpie CAS shader uses the declared R8 path",
		};
    }

    if (!IsGroupA(effectName)) {
        return {};
    }

    const char* auxiliary = "none";
    const char* evidence = "local shader declaration; public HDR protocol unspecified";
    if (effectName == "Anime4K") {
        auxiliary = "variant-specific R16G16B16A16_FLOAT CNN surfaces; Thin_HQ R16G16_FLOAT gradient";
    } else if (effectName == "CRT") {
        auxiliary = "GTU_v050 tex1 R16G16B16A16_FLOAT; other presets none";
    } else if (effectName == "CuNNy") {
        auxiliary = "model-specific R8G8B8A8_SNORM t0..t7";
    } else if (effectName == "CuNNy2") {
        auxiliary = "model-specific R8G8B8A8_UNORM T0..T15";
    } else if (effectName == "FSRCNNX") {
        auxiliary = "featureMap1/2 and tex1..tex4 R16G16B16A16_FLOAT";
    } else if (effectName == "MLAA") {
        auxiliary = "edgeMask R8G8_UNORM; edgeCounts R8G8B8A8_UNORM";
    }

    HdrFormatRoutes routes;
    const HdrAlphaMode alpha = effectName == "Diagnostics" || effectName == "MLAA"
        ? HdrAlphaMode::Preserve : HdrAlphaMode::ForceOpaque;
    routes.push_back(MakeSdrFallback(effectName, "unknown-sdr-fallback", DXGI_FORMAT_R8G8B8A8_UNORM, alpha));
    return {
        .routes = std::move(routes),
        .auxiliaryResources = auxiliary,
        .evidence = evidence,
    };
}

HdrFormatRoutes GetGroupAHdrRoutes(std::string_view effectName, int casFormatOption) {
    return GetGroupAHdrEffectDescription(effectName, casFormatOption).routes;
}

}
