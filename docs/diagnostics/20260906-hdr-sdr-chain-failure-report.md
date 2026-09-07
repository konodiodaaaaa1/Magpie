# HDR / SDR chain failure report

## Evidence

- Runtime logs show `HDR diagnostics` and repeated adapter dispatches, so the HDR front end is active.
- Runtime logs do not show a texture readback or presentation-pixel statistic; visual output cannot be inferred from successful `EvaluateFeature` calls.
- The prior external route report produced identical values for every effect and was removed because it did not execute production code.
- `Magpie-src` still contains separate legacy `FSR2ZeroMVUpscaler`, `FSR3ZeroMVUpscaler`, and `XeSSZeroMVUpscaler` implementations. The research tree replaced these with different classes while retaining legacy effect names in configuration.

## Confirmed regression sources

1. The research `NativeEffectBackendFactory` routes legacy ZeroMV/Jitter/OpticalFlow effect IDs through the new SR wrappers. This changes the SDR resource and auxiliary-input contract even when HDR compatibility is disabled.
2. `EffectDrawer` adds an HDR wrapper around every effect, while the original SDR drawer must remain byte-for-byte equivalent when HDR is off. Any stale HDR boundary or `_hdrOutput` resource can therefore affect the common output pointer.
3. `PassThroughFrames` and the processed publication path have separate presentation conversions; both need an identical output contract before comparison mode is considered valid.
4. `HdrSurfaceAdapter` is a shared compute path. Its constant-buffer layout, input/output descriptors, transfer mode, and dispatch completion must be validated by GPU readback before any effect result is trusted.

## Required correction order

1. Restore the original SDR backend class selection and `EffectDrawer` execution path as the off-state baseline.
2. Add a production-owned test bridge inside `Magpie.Core` that runs one captured texture through the actual `EffectDrawer` and reads back each boundary texture.
3. Re-enable HDR per boundary only after the off-state bridge passes for every configured effect group.
4. Validate `HdrCaptureProcessor`, `HdrSurfaceAdapter`, pass-through, shared publication, presenter, and screenshot export as separate GPU stages.
5. Mark a matrix row passed only when the production path returns a non-black, finite image whose hash differs from the source and whose stage metadata matches the selected route.
