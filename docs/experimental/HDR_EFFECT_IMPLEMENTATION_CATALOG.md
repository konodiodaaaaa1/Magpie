# HDR Effect Implementation Catalog

## Purpose

This catalog translates the protocol evidence matrix into an implementation-facing list. It records accepted backend protocols separately from HDR-native protocols, which adapter profile HDR mode should choose, what conversion happens at the boundary, and which conclusions still need local testing.

The authoritative source URLs and research notes are in [HDR_PROTOCOL_EVIDENCE.md](HDR_PROTOCOL_EVIDENCE.md) and [HDR_PROTOCOL_EVIDENCE.json](HDR_PROTOCOL_EVIDENCE.json). This catalog records integration decisions; it does not claim that current Magpie already supports these paths.

## Profile Legend

| Profile | Meaning at the HDR adapter boundary |
|---|---|
| DirectFP16 | Backend has a documented or sufficiently verified floating-point HDR-capable route. Canonical FP16 stays FP16 through the backend boundary. |
| BoundedHDR | Backend needs a bounded HDR domain or a specific display representation. Use a named normalizer or encoder and its paired inverse/presentation policy. |
| SDRCompatible | Backend contract is SDR, UNORM, U8, or another bounded SDR domain. Convert canonical HDR to a defined SDR proxy and restore into the canonical domain afterward. |
| ConditionalFP16 | A reference implementation or user-shader environment uses FP16 while a general HDR color contract remains unproven. Keep this opt-in and experimental. |
| Unknown | No stable upstream mapping or format contract. Keep HDR use behind a local validation gate. |
| PresentationTerminal | Backend produces or owns presentation frames. It ends an effect branch and does not return a normal canonical texture. |

## Protocol Option Semantics

The canonical inter-effect surface is always `R16G16B16A16_FLOAT`. This does not limit the format used inside an effect boundary. A concrete backend may expose multiple selectable protocol options when each option has reliable evidence:

| Backend option | HDR-mode meaning | Adapter behavior |
|---|---|---|
| `R16G16B16A16_FLOAT` | Candidate HDR-native linear/scene-referred route; transfer and range still need metadata | Direct FP16 or bounded-HDR adapter according to the contract |
| `R10G10B10A2_UNORM` | Bounded HDR10/PQ route when the backend explicitly defines BT.2100/HDR10 semantics | Encode PQ/BT.2100 and preserve terminal metadata |
| `R8G8B8A8_UNORM` | Usually SDR-compatible storage, even when accepted by an HDR-capable effect | HDR-to-SDR mapping, clamp/quantize, backend call, paired reconstruction |

The existence of an R8 and FP16 option means the effect has two selectable backend routes. It does not make the R8 route HDR-native. The implementation must therefore distinguish `acceptedFormatRoutes`, `hdrNativeFormatRoutes`, and `hdrAdapterFormatRoutes` for every effect.

```json
{
  "acceptedFormatRoutes": ["R8G8B8A8_UNORM", "R16G16B16A16_FLOAT"],
  "hdrNativeFormatRoutes": ["R16G16B16A16_FLOAT"],
  "hdrAdapterFormatRoutes": ["R8G8B8A8_UNORM"]
}
```

An effect-level selector is displayed only when more than one reliable route is available. The selector controls the effect boundary protocol, while all preceding and following effects continue to exchange canonical FP16. SDR mode keeps the current default and does not inherit HDR selector behavior.

## Integration Summary

| Effect group / backend | Known accepted resources | HDR adapter decision | Priority |
|---|---|---|---|
| Anime4K | Public texture/color contract unavailable | Unknown | Later |
| CAS | Reference CLI: `R8G8B8A8_UNORM` and `R16G16B16A16_FLOAT` | Two selectable routes: FP16 candidate HDR-native; R8 SDR-compatible | High |
| CRT, CuNNy, CuNNy2, Diagnostics | No unique public API contract | Unknown | Later |
| DLSS SR | HDR color path; depth, MV, exposure, history resources | DirectFP16 | High |
| DLSS FG | Backbuffer/HUD/UI share format and color space | BoundedHDR + PresentationTerminal | Later |
| DLSSNR | Local bounded-HDR experiment is validated | BoundedHDR | First vertical slice |
| FSR1 | Perceptual sRGB `[0,1]` core contract | SDRCompatible | Medium |
| FSR2, FSR3 SR, FSR4 | HDR SR interfaces with temporal auxiliary inputs | DirectFP16 | High |
| FSR3 FG | Backbuffer, HUD-less, MV, depth, optical flow | BoundedHDR + PresentationTerminal | Later |
| FSRCNNX, FXAA, MLAA, Pixel Art, Sharpen | No stable generic format contract | Unknown | Later |
| NIS | Non-integer color resources; PQ and linear HDR modes | DirectFP16 | High |
| NNEDI3, RAVU | mpv `rgba16f/rgba16hf` implementation evidence | ConditionalFP16 | Medium |
| RTX Video VSR/Denoiser | GPU BGRA/RGBA interleaved U8 | SDRCompatible | High |
| RTX Video HDR | Product behavior known; public API texture contract unavailable | Unknown | Do not integrate yet |
| SMAA, xBRZ | RGBA / U8-class resource contracts | SDRCompatible | Medium |
| XeSS SR | `RGBA16F`, `R11G11B10F`, `RGBA8` and linear formats | DirectFP16 | High |
| XeSS FG | HDR10/BT.2100 `R10G10B10A2_UNORM`; no FP16 HDR/scRGB | BoundedHDR + PresentationTerminal | High after core path |
| NVIDIA Optical Flow | `GRAYSCALE8`, `NV12`, `ABGR8`; flow output | SDRCompatible auxiliary path | Medium |
| AMD FidelityFX Optical Flow | Color format not publicly enumerated; flow output | BoundedHDR auxiliary path | Later |

## Direct FP16 Core Effects

### DLSS Super Resolution

- **Input:** HDR-capable application color buffer, motion vectors (`RG16F` or `RG32F`), depth, optional `R16F` exposure, and temporal controls.
- **Output:** Application output buffer; history output is commonly `RGBA16F`.
- **Adapter:** Keep canonical FP16 color and convert only required primary/transfer semantics. Allocate native-format auxiliary resources separately.
- **Boundary:** Color supports FP16. Full temporal quality still needs genuine render-space MV, depth, exposure, jitter, reset, and history behavior; desktop capture does not create equivalent data.

### FSR2, FSR3 Super Resolution, and FSR4

- **Input:** Application-specified color buffer, one-channel depth, two-component motion vectors, exposure, and masks where required.
- **Output:** Application output resource; documented implementations use FP16 internal surfaces.
- **Adapter:** Keep color in canonical FP16 and provide native auxiliary inputs only where external code can supply valid data.
- **Boundary:** Their HDR color route and their temporal resource contract are independent constraints.

### NVIDIA Image Scaling

- **Input/output:** Non-integer color resources; documented LDR, PQ, and linear-HDR modes.
- **Adapter:** DirectFP16 with the chosen NIS HDR range, sampler, and resource-state contract stored in its protocol.
- **Boundary:** Spatial behavior makes NIS a practical early external HDR target.

### XeSS Super Resolution

- **Input:** `R16G16B16A16_FLOAT`, `R11G11B10_FLOAT`, `R8G8B8A8_UNORM`, and supported linear formats; MV and depth have native formats.
- **Output:** Same format and color space as the input color texture; alpha requires canonical cleanup.
- **Adapter:** Use canonical FP16 by default. A later `R11G11B10_FLOAT` option needs verified alpha-free usage.
- **Boundary:** Temporal auxiliary inputs remain independent from HDR color compatibility.

## Conditional FP16 Effects

### CAS

- **Known resources:** The official reference CLI uses `R8G8B8A8_UNORM` and `R16G16B16A16_FLOAT`; shader paths include linear handling and FP16/FP32 math variants.
- **Adapter:** Start with the current SDR-compatible selection. Add a hidden or experimental DirectFP16 profile after the exact Magpie CAS path proves that it preserves linear FP16 values and alpha.
- **Boundary:** Reference CLI behavior proves a viable FP16 direction. It does not alone prove every CAS integration's resource contract.

### NNEDI3

- **Known resources:** mpv user-shader variants use `rgba16f/rgba16hf` internal surfaces and focus on luma.
- **Adapter:** Treat FP16 as implementation-format capability. Feed a defined luma or RGB projection only after identifying the exact variant; restore alpha and color semantics at the boundary.
- **Boundary:** No public native HDR color contract is established.

### RAVU

- **Known resources:** mpv variants use `rgba16f/rgba16hf`; RGB, YUV, luma-only, gather, and compute variants differ.
- **Adapter:** Bind protocol to the exact variant. RGB variants can be FP16 candidates; YUV and luma paths require explicit color/luma conversion.
- **Boundary:** FP16 surface availability does not define PQ, HLG, scene-linear range, or metadata behavior.

## Bounded HDR and Frame-Generation Paths

### DLSSNR

- **Known local result:** Direct HDR texture values create color failure, including red leakage. A bounded HDR normalization path produces the intended result.
- **Required chain:** `canonical FP16 HDR -> normalize -> DLSSNR -> inverse normalize -> canonical FP16 HDR`.
- **Adapter state:** Curve identity, scale, reference white, peak/headroom, alpha rule, source color assumption, and inverse parameters.
- **Boundary:** This is a local experimental protocol. It stays separate from official vendor-contract claims and needs capture/output diagnostics.

### FSR3 Frame Generation

- **Known resources:** Presentation backbuffer, optional HUD-less buffer, depth, MV, and optical-flow resources.
- **Adapter:** Treat it as a bounded presentation path whose swap-chain format and color semantics agree across composed inputs.
- **Boundary:** Frame generation needs temporal and motion information unavailable from a simple desktop image. Color adaptation alone does not satisfy the backend contract.

### XeSS Frame Generation

- **Known resources:** HDR uses `R10G10B10A2_UNORM` with HDR10 / BT.2100. HUD-less and UI resources match backbuffer format, color space, and size. UI alpha is explicit.
- **Adapter:** Encode canonical FP16 to PQ/BT.2100 `R10G10B10A2_UNORM`, run XeSS FG, then present through its proxy swap chain.
- **Boundary:** The documented HDR route excludes FP16 HDR and scRGB. XeSS FG ends the ordinary effect chain.

### DLSS Frame Generation

- **Known resources:** Backbuffer, HUD-less/UI resources, MV/depth, and an output texture matching backbuffer format.
- **Adapter:** Keep behind a presentation-terminal abstraction until the exact external format profile is verified.
- **Boundary:** Current evidence establishes same-format behavior and HDR capability while a complete external texture matrix remains unresolved.

### AMD FidelityFX Optical Flow

- **Known resources:** Color input feeds a transfer/luminance conversion; outputs include `R16G16_SINT` flow vectors and `R32_UINT` scene-change data.
- **Adapter:** Use a bounded-HDR luminance adapter after a local test identifies source range and transfer semantics.
- **Boundary:** Optical flow is an auxiliary-resource producer, not a normal RGB effect output.

## SDR-Compatible Effects

### FSR1

- **Known resources:** Core input uses perceptual/sRGB `[0,1]`; RCAS has invalid behavior for negative values.
- **Adapter:** `canonical FP16 HDR -> SDR appearance mapping -> sRGB [0,1] -> FSR1 -> SDR decode/reconstruction`.
- **Boundary:** Surrounding helper code contains HDR conversion utilities, while the core algorithm contract remains SDR oriented.

### RTX Video VSR

- **Known resources:** GPU-resident BGRA or RGBA interleaved U8 input/output.
- **Adapter:** Convert canonical HDR to selected SDR appearance, encode SDR transfer, and quantize each channel to U8 `0..255`. Convert output back through the paired reconstruction policy.
- **Boundary:** U8 is a hard precision boundary. The public interface has no native HDR transfer, primary, metadata, or FP16 contract.

### RTX Video Denoiser

- **Known resources:** Same BGRA/RGBA U8 GPU-buffer interface; denoise/deblur output stays at input resolution.
- **Adapter:** Use the same SDR/U8 protocol as VSR, with no scaling assumption.
- **Boundary:** Keep VSR and Denoiser protocol identifiers separate despite shared encoding.

### SMAA, xBRZ, and NVIDIA Optical Flow

- **SMAA:** RGBA color texture plus edge/area/search/depth resources. Intermediate textures are normally non-sRGB; final neighborhood blending can be sRGB.
- **xBRZ:** Community TypeScript/WASM path uses RGBA U8 `Uint8ClampedArray` with alpha and scale factors 2 through 6.
- **NVIDIA Optical Flow:** `GRAYSCALE8`, `NV12`, or `ABGR8` input; flow output is signed fixed-point `SHORT2`.
- **Adapter:** Use SDR-compatible conversion. Optical-flow vector outputs remain outside the canonical RGB chain.

## Effects Awaiting a Stable Public Contract

### Anime4K

- **Known resources:** The shader family is distributed as user shaders with variant-specific texture declarations; one universal color-format contract is not established.
- **Adapter:** Keep `Unknown` until the selected preset is inspected. If the preset declares normalized SDR sampling, use `SDRCompatible`; if it declares floating-point linear sampling, promote it to `ConditionalFP16` after a range test.
- **Boundary:** Do not infer HDR support from shader compilation. Record texture format, sampler state, transfer assumption, and whether negative or above-one values survive each pass.

### CRT

- **Known resources:** CRT presets combine color, scanline, mask, and sometimes feedback passes; the exact format and transfer behavior depend on the preset.
- **Adapter:** Treat each preset as an independent protocol. Default to `SDRCompatible` and require a per-pass range audit before allowing direct FP16 HDR.
- **Boundary:** Scanline and mask math can be visually valid for SDR while clipping HDR highlights or changing saturated colors.

### CuNNy and CuNNy2

- **Known resources:** Neural shader models use model-specific channel, normalization, and tensor-size assumptions; a single public HDR contract is not established.
- **Adapter:** Keep `Unknown`. A future adapter must identify the model's normalization interval, tensor format, channel order, and output denormalization before enabling HDR.
- **Boundary:** Treat model normalization as part of the protocol. A generic FP16 surface alone does not establish that the network accepts HDR values.

### Diagnostics

- **Known resources:** Diagnostic effects may inspect or rewrite channels, ranges, alpha, or color-space metadata depending on the diagnostic selected.
- **Adapter:** Run diagnostics on the canonical FP16 surface where possible, while preserving a raw-capture tap and the pre-adapter color description.
- **Boundary:** Diagnostics are not an image-quality backend. Their output must never be silently reused as a normal HDR frame unless the selected diagnostic explicitly promises that behavior.

### FSRCNNX

- **Known resources:** User-shader variants expose model- and preset-specific texture declarations; no stable general HDR input/output contract is established.
- **Adapter:** Default to `SDRCompatible`. Permit `ConditionalFP16` only for a tested variant whose normalization and output range are documented.
- **Boundary:** Verify RGB versus luma-only operation, alpha handling, and any hard clamp in every model pass.

### FXAA and MLAA

- **Known resources:** Both are neighborhood-based anti-aliasing families commonly implemented over normalized color textures, with auxiliary edge/luma data varying by implementation.
- **Adapter:** Use `SDRCompatible` until the exact implementation proves unclamped FP16 behavior. Keep edge/luma auxiliary resources separate from the canonical RGB surface.
- **Boundary:** Anti-aliasing thresholds are often tuned in display-referred units; direct scene-linear HDR can change edge detection even when the texture format is floating point.

### Pixel Art

- **Known resources:** Presets vary between integer nearest-neighbor logic, palette tests, and shader-specific color thresholds.
- **Adapter:** Default to `SDRCompatible`; allow direct FP16 only for a preset whose comparisons are explicitly range-independent.
- **Boundary:** Preserve exact alpha and integer-like color comparisons. Do not apply an HDR tone curve inside the pixel-art backend without recording it as a deliberate artistic transform.

### RTX Video HDR

- **Known resources:** Product-level HDR behavior is known, while a public external texture/API contract suitable for this effect catalog remains unresolved.
- **Adapter:** Keep `Unknown` and do not route canonical frames into it by assumption. Integrate only after the exact API, accepted resource formats, color space, metadata, and output ownership are documented.
- **Boundary:** A display feature or driver capability is not evidence of an externally callable native-HDR filter contract.

### Sharpen

- **Known resources:** The name covers multiple backends and shader families, including normalized SDR and floating-point implementations.
- **Adapter:** Resolve by concrete backend identifier. Use `SDRCompatible` for normalized SDR variants and `ConditionalFP16` for variants with verified unclamped linear-FP16 behavior.
- **Boundary:** Negative lobes, overshoot clamps, alpha treatment, and sharpening strength units must be recorded because they directly affect HDR highlight reconstruction.

## Uniform Runtime Order

When HDR compatibility is enabled, every ordinary effect branch follows this order:

1. Capture with the existing capture method and retain source color metadata.
2. Decode the captured transfer function and normalize exposure into canonical `RGBA16F`.
3. Select the effect profile from the concrete backend identifier and evidence level.
4. Convert canonical HDR into the backend's declared input protocol.
5. Run the existing backend without changing its SDR-mode behavior.
6. Convert the backend output back into canonical `RGBA16F`, restoring alpha according to the profile.
7. Continue to the next effect, or hand the terminal presentation branch to its required swap-chain format.
8. Encode the final canonical frame for the existing presentation path and apply metadata at presentation time.

If a backend has no reliable protocol evidence, use the existing compatible SDR route and record the conversion. Unknown effects must not silently receive direct HDR values.

## Runtime Diagnostic Record

Each adapted effect should emit a compact record containing:

```text
effect identifier
selected profile and evidence level
input DXGI format and ColorDescription
adapter input conversion and numeric range
backend texture formats and resource states
backend output format and range
adapter output conversion
alpha rule and auxiliary-resource formats
GPU time: pre-adapter / backend / post-adapter
observations: highlights / saturated red / UI white / dark gradients
```

The record should also include whether the path was direct, bounded, SDR-compatible, or unknown-fallback, plus the normalizer identity and parameters when a bounded-HDR path is used.

## Validation Matrix

| Test stage | Required check | Pass condition |
|---|---|---|
| Capture | HDR transfer decode and exposure | No highlight expansion caused by treating HDR code values as SDR |
| Single effect | Adapter round trip | Neutral gray, saturated red, white UI, and dark gradients remain stable |
| Chained effects | Repeated boundary conversions | No progressive hue drift, highlight pumping, or alpha loss |
| Temporal effect | History, MV, depth, and exposure agreement | All temporal inputs use the same frame color description and dimensions |
| Presentation terminal | Swap-chain and metadata ownership | Output format and metadata match the terminal backend contract |
| SDR regression | HDR option disabled | Existing SDR path and output remain unchanged |

## Implementation Priority

1. Implement the canonical capture-to-`RGBA16F` HDR front end and make it independently toggleable.
2. Integrate direct FP16 SR effects whose contracts are already sufficiently established.
3. Integrate bounded adapters for DLSSNR and explicitly bounded temporal/presentation paths.
4. Integrate SDR-compatible VSR, Denoiser, FSR1, SMAA, xBRZ, and optical-flow auxiliary paths.
5. Add per-backend selectors only where multiple accepted protocols have reliable evidence.
6. Keep unknown effects behind diagnostics and promote them only after a concrete preset/backend audit.

## Current Non-Goals

- Replacing any backend model or retraining a neural effect.
- Changing the existing SDR path when HDR compatibility is disabled.
- Treating `R16G16B16A16_FLOAT` as a color space without transfer and gamut metadata.
- Claiming native HDR support for an effect from format enumeration, compilation success, or product-level marketing behavior alone.
