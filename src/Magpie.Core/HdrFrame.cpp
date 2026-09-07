#include "pch.h"
#include "HdrFrame.h"

#include <cmath>

namespace Magpie {

bool HdrMetadata::HasLuminanceRange() const noexcept {
    return maxMasteringLuminanceNits > 0.0f && minMasteringLuminanceNits >= 0.0f &&
        maxMasteringLuminanceNits >= minMasteringLuminanceNits;
}

bool ColorDescription::IsValid() const noexcept {
    return primaries != HdrColorPrimaries::Unknown &&
        transfer != HdrTransferFunction::Unknown &&
        std::isfinite(referenceWhiteNits) && referenceWhiteNits > 0.0f &&
        std::isfinite(sdrWhiteNits) && sdrWhiteNits > 0.0f &&
        std::isfinite(displayPeakNits) && displayPeakNits >= referenceWhiteNits &&
        std::isfinite(preExposure) && preExposure > 0.0f;
}

bool HdrFrameMetadata::IsValid() const noexcept {
    return valid && width != 0 && height != 0 && color.IsValid() &&
        stage != HdrFrameStage::Unknown;
}

bool HdrFrame::IsCanonical() const noexcept {
    if (!texture || !metadata.IsValid() ||
        workingFormat != DXGI_FORMAT_R16G16B16A16_FLOAT) {
        return false;
    }

    D3D11_TEXTURE2D_DESC textureDesc{};
    texture->GetDesc(&textureDesc);
    return textureDesc.Format == workingFormat &&
        textureDesc.Width == metadata.width && textureDesc.Height == metadata.height;
}

std::string HdrFormatRoute::Id() const {
    if (effectId.empty()) {
        return optionId;
    }
    if (optionId.empty()) {
        return effectId;
    }
    return effectId + "/" + optionId;
}

bool HdrFormatRoute::IsValid() const noexcept {
    if (effectId.empty() || optionId.empty()) {
        return false;
    }
    if (inputFormat == DXGI_FORMAT_UNKNOWN || outputFormat == DXGI_FORMAT_UNKNOWN) {
        return false;
    }
    if (adapterProfile == HdrAdapterProfile::Unknown) {
        return false;
    }
    return alphaMode != HdrAlphaMode::Unknown;
}

}
