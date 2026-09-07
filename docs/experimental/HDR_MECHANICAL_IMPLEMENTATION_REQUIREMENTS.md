# HDR Mechanical Implementation Requirements

## Purpose

This document assigns the repetitive, locally verifiable work for the first HDR architecture slice. The global design decisions remain in [HDR_COMPATIBILITY_ARCHITECTURE.md](HDR_COMPATIBILITY_ARCHITECTURE.md). This task covers configuration plumbing, protocol data plumbing, reusable conversion dispatch, diagnostics, and the generic effect-boundary execution layer. It does not alter concrete effect shader sources or model implementations.

## Required configuration plumbing

Add the profile-level general setting:

```json
"enableHdrCompatibility": false
```

The setting must map to `ScalingFlags::EnableHdrCompatibility` and expose `IsHdrCompatibilityEnabled()` in `ScalingOptions`. Preserve unknown configuration fields and preserve the default-off behavior for existing profiles. Add the setting to profile copy, load, save, and runtime option construction. Add the corresponding general-settings UI binding and localization using the repository's existing conventions.

## Required protocol data plumbing

Extend the reusable HDR protocol description so an effect can later declare multiple backend routes without changing the canonical work surface. Each route must be able to store:

```text
effectId
optionId
inputFormat
outputFormat
inputTransfer
outputTransfer
inputRange
outputRange
alphaMode
evidenceLevel
hdrNative
adapterProfile
defaultForHdr
defaultForSdr
```

Do not add unverified effect-specific routes in this task. Add storage, validation, default selection, serialization helpers, and the generic structured SDR fallback route used when a concrete route is absent.

## Required generic conversion dispatch

Implement a reusable adapter dispatcher with these profiles:

```text
DirectFP16
BoundedHDR
SDRCompatible
ConditionalFP16
Unknown
PresentationTerminal
```

The dispatcher must accept canonical `R16G16B16A16_FLOAT` input and return canonical `R16G16B16A16_FLOAT` output for non-terminal routes. It must select conversion behavior from the route description rather than from effect-name string comparisons. The SDR-compatible route must use paired HDR-to-SDR and SDR-to-HDR parameters and preserve alpha explicitly. Unknown routes must select the existing compatible fallback and emit a diagnostic state.

## Required capture-front-end hook

Add a reusable `HdrCaptureProcessor` integration point after any selected frame source produces its texture. Keep Graphics Capture, Desktop Duplication, GDI, and DwmSharedSurface selection unchanged. The processor must expose:

```text
Process(sourceTexture, sourceFormat, sourceColorDescription)
GetCanonicalTexture()
GetFrameMetadata()
ResetForResize()
```

The implementation may use a display-derived source color description where the capture API does not expose texture metadata, but it must mark and record that inference. The processor must use reusable textures and must not replace the existing SDR texture when HDR compatibility is disabled.

When HDR compatibility is enabled, the delivery path must retain the canonical FP16/scRGB surface through shared publication and presenter composition. R8 publication remains the disabled-mode path and is not used as the terminal HDR representation.

## Required diagnostics

Add structured logging for:

```text
HDR option state
capture method
source format
source color description
canonical format
selected adapter profile
selected route id
conversion path
fallback reason
```

Avoid per-pixel CPU readback. Do not add a staging readback path to production rendering.

## Explicit exclusions

- No changes to concrete effect shader sources or model implementations. Generic boundary calls may wrap their existing backend resources when HDR compatibility is enabled.
- No model changes, shader-model changes, or effect shader rewrites.
- No change to SDR behavior when the global HDR option is disabled.
- No hard-coded assumption that every FP16 texture is HDR.
- No claim that a format enumeration proves HDR support.

## Acceptance checks

1. Existing profiles load with HDR disabled.
2. The general settings control round-trips through profile save/load.
3. The selected capture method is unchanged with HDR enabled.
4. HDR mode exposes a canonical FP16 frame descriptor after capture.
5. SDR mode bypasses the HDR processor and retains the previous resource path.
6. The adapter dispatcher has unit-level coverage for all six profiles.
7. The SDR-compatible adapter uses paired forward/inverse parameters.
8. No concrete effect file is modified.
9. The project files parse and the touched targets compile.

## Deliverables

Return the changed file list, configuration key, runtime flag, route serialization shape, adapter dispatch entry point, and validation commands. Report any capture API metadata assumption explicitly.
