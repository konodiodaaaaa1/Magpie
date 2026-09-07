# Magpie 0.6.5 HDR Effect Protocol Audit

Date: 2026-09-07  
Canonical surface: `R16G16B16A16_FLOAT`, linear scRGB, reference white 80 nit.

This audit separates source-level protocol evidence from a live SDK result. A
route is marked `Live` only when the current native backend performs its
production Draw/Evaluate/Run call and the renderer records the resulting
texture. A route marked `Contract` has source and SDK evidence but still needs
a target-machine pixel capture. `Fallback` means the canonical bridge is
implemented and the effect remains on the explicit compatibility path.

## Effect Matrix

| effectId / variant | selected HDR protocol | backend call | status | evidence / blocking item |
|---|---|---|---|---|
| DLSS SR | DirectFP16, linear | `DLSSSRUpscaler::Draw` -> NGX Evaluate | Contract | zero MV/depth contract; HDR flag/exposure still needs live pixel proof |
| FSR1 | SDRCompatible, sRGB R8 | shader pass | Fallback | FSR1 core is bounded perceptual sRGB |
| FSR2 | DirectFP16, linear | `FSR2Upscaler::Draw` -> FSR2 dispatch | Contract | HDR flag/exposure/depth are effect-local open items |
| FSR3 SR | DirectFP16, linear | `FSR3Upscaler::Draw` -> FSR3 dispatch | Contract | current provider path still needs HDR linear dispatch proof |
| FSR4 | DirectFP16, linear | `FSR3Upscaler::Draw(useFsr4)` | Contract | FSR4 ML/provider contract and live output proof pending |
| XeSS SR | DirectFP16, FP16 linear (default) | `XeSSUpscaler::Draw` -> `xessD3D12Execute` | Contract | `XESS_INIT_FLAG_LDR_INPUT_COLOR` is now omitted for FP16/R10 HDR routes |
| NIS | DirectFP16 candidate | shader pass using NIS linear HDR mode | Contract | exact Magpie `NIS_HDR_MODE` compile route and live pixel proof pending |
| CAS | SDRCompatible R8 | shader pass | Fallback | current shader saturates RGB and forces alpha 1 |
| Anime4K | Unknown -> SDR fallback | shader passes | Fallback | preset-specific FP16 intermediates lack a color contract |
| CRT | Unknown -> SDR fallback | shader passes | Fallback | preset gamma/clamp behavior is SDR-bound |
| CuNNy | Unknown -> SDR fallback | shader passes | Fallback | SNORM model tensors and normalization are unverified HDR |
| CuNNy2 | Unknown -> SDR fallback | shader passes | Fallback | UNORM model tensors and normalization are unverified HDR |
| FSRCNNX | Unknown -> SDR fallback | shader passes | Fallback | FP16 intermediates do not define input/output HDR semantics |
| FXAA | Unknown -> SDR fallback | shader passes | Fallback | luma threshold has no HDR scale contract |
| MLAA | Unknown -> SDR fallback | shader passes | Fallback | R8 edge/count auxiliaries are bounded SDR resources |
| SMAA | SDRCompatible R8/FP16 auxiliaries | shader passes | Fallback | alpha and luma threshold are variant-specific |
| xBRZ | SDRCompatible R8 | shader passes | Fallback | integer-like RGB comparisons and alpha=1 |
| Pixel Art | SDRCompatible R8 | shader passes | Fallback | MMPX/Pixellate force alpha 1; SharpBilinear preserves it |
| Sharpen series | SDRCompatible normalized RGB | shader passes | Fallback | no stable upstream HDR contract |
| DLSSNR | R8 user path: SDRCompatible; experimental path: BoundedHDR FP16 | `DLSSNRFilter::Draw` -> Feature 18 Evaluate | Contract | R8 selection remains explicit; FP16 scale is local experiment, not vendor HDR proof |
| RTXVideo VSR Ultra | SDRCompatible `R8G8B8A8_UNORM` endpoint | `NvCVImage_Transfer` -> `NvVFX_Run` -> output transfer | Contract | U8 interleaved 0..255; `RTXVideoVSR/VSR-U8-sRGB-255` route |
| RTXVideo Denoise Ultra | SDRCompatible `R8G8B8A8_UNORM` endpoint | `NvCVImage_Transfer` -> `NvVFX_Run` -> output transfer | Contract | independent `RTXVideoDenoise/Denoise-U8-RGBA` route; same-resolution rule |
| NVIDIA Optical Flow | SDRCompatible ABGR8 auxiliary | `nvOFExecute` | Contract | output is S10.5 flow plus confidence; never a canonical RGB surface |
| AMD Optical Flow | BoundedHDR auxiliary candidate | FidelityFX optical-flow dispatch | Contract | current input is RGBA8 with fixed sRGB/luminance parameters |
| DLSSFG | PresentationTerminal record | `DLSSFrameGenerator::Draw` / publication | Contract | SDK backbuffer/HUD/UI HDR format and color-space contract still runtime dependent |
| XeSSFG | PresentationTerminal HDR10 R10 | XeSS-FG proxy swap chain + `Present` | Contract | R10 resource and `DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020` are explicit; live capture pending |
| FSR3/FSR4 FG | PresentationTerminal record | no native backend in current tree | Unavailable | protocol record exists; production backend and SDK are missing |
| third-party native backends | per-backend route above | factory-dispatched native `Draw` | Mixed | only VFX, DLSS/FSR/XeSS and optical-flow providers have callable native paths |

## Required Protocol Record

Every route is represented by `HdrFormatRoute` and serialized through
`SerializeHdrFormatRoute`. The runtime diagnostic emitted at capture and native
failure boundaries contains the effect route/profile, source format, source
color description, canonical format, conversion path and fallback reason. The
full per-frame evidence record is:

```text
effectId, effectVariant, selectedProtocol, profile, evidenceLevel,
inputFormat, inputColorModel, inputTransfer, inputRange,
backendInputFormat, backendOutputFormat, outputTransfer, alphaRule,
auxiliaryResources, normalizationParameters, nativeInitialization,
nativeDrawOrEvaluate, nativeResultCode, outputFiniteStats,
canonicalOutputStats, gpuTime, screenshot, sha256
```

`HdrSurfaceAdapter` keeps the paired forward/inverse parameters for
SDR-compatible and bounded routes. `HdrCaptureProcessor` produces and reuses
the canonical FP16 surface after the selected capture source. `EffectDrawer`
executes the adapter before and after the real shader/native boundary; native
failure is logged with an explicit marker-pass fallback.

## DLSSNR Decision

The user-selected R8 route remains `DLSSNR/sdr-r8` and allocates R8 input and
output resources. The experimental FP16 route is selected only when
`experimentalHdrPath` is enabled and the validated scale is `1`, `2`, or `4.5`;
it allocates FP16 resources and records `BoundedHDR` with the scale. No route
selection branch promotes the FP16 experiment when the user selected R8.

The current implementation records the normalization scale, reference white,
display peak and adapter mode. Existing local experiments prove the real
Feature 18 Create/Evaluate path for the tested DLL/driver combinations, while
the exact vendor normalization curve remains an open experimental parameter.

## RTX Video Decision

VSR and Denoise now have separate protocol IDs. Both use an explicit U8,
interleaved RGBA/BGRA endpoint with values in `[0,255]`. The native backend
records `NvCVImage_InitFromD3D11Texture`, input transfer, `NvCVImage_Transfer`,
`NvVFX_Run`, output transfer and synchronization. A native failure stays a
failure in the log and the renderer marks the compatibility marker pass
separately.

## Evidence Gaps

The current workspace has no reproducible live GPU capture for every listed
effect, so the effects marked `Contract`, `Fallback`, or `Unavailable` are not
reported as visually passed. The existing `E:\Magpie-0.6.5-build\matrix`
captures and DLSSNR experiment logs remain the available pixel/hash evidence.
The startup matrix must be rerun on a target display with the user sample to
populate the remaining screenshot, SHA-256, finite-statistics, and GPU-time
fields.

