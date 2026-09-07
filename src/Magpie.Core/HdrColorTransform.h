#pragma once

#include "HdrFrame.h"
#include <array>

namespace Magpie {

using HdrColor = std::array<float, 4>;

struct HdrTransformParameters {
    float exposure = 1.0f;
    // Canonical scRGB reference white is fixed by the global HDR protocol.
    float referenceWhiteNits = 80.0f;
    float sdrWhiteNits = 80.0f;
    float hdrPeakNits = 1000.0f;
    float shoulder = 1.0f;
    bool preserveAlpha = true;
    bool IsValid() const noexcept;
};

struct HdrTransformConstants {
    float exposure = 1.0f;
    float inverseExposure = 1.0f;
    float sdrWhiteScale = 0.08f;
    float hdrPeakNits = 1000.0f;
    float shoulder = 1.0f;
    uint32_t inputTransfer = static_cast<uint32_t>(HdrTransferFunction::Linear);
    uint32_t outputTransfer = static_cast<uint32_t>(HdrTransferFunction::Linear);
};

class HdrColorTransform {
public:
    static HdrTransformParameters ForFrame(const ColorDescription& color) noexcept;
    static HdrTransformConstants PrepareConstants(
        const HdrTransformParameters& parameters,
        HdrTransferFunction inputTransfer,
        HdrTransferFunction outputTransfer
    ) noexcept;
    static float DecodeTransfer(float value, HdrTransferFunction transfer) noexcept;
    static float EncodeTransfer(float value, HdrTransferFunction transfer) noexcept;
    static float MapHdrToSdr(float value, const HdrTransformParameters& parameters) noexcept;
    static float MapSdrToHdr(float value, const HdrTransformParameters& parameters) noexcept;
    static HdrColor Transform(
        const HdrColor& color,
        HdrTransferFunction inputTransfer,
        HdrTransferFunction outputTransfer,
        const HdrTransformParameters& parameters
    ) noexcept;
};

}
