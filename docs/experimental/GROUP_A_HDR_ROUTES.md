# Group-A HDR routes

`GroupAHdrRoutes.{h,cpp}` is the effect-local provider for Anime4K, CAS, CRT,
CuNNy, CuNNy2, Diagnostics, FSRCNNX, FXAA and MLAA. It returns structured
`HdrFormatRoute` values plus a diagnostic description of auxiliary resources.
The provider is selected by the effect group prefix in the existing Renderer
boundary update. No effect-name branches were added to the shared dispatcher.

Every group-A effect has a named route with explicit `R8G8B8A8_UNORM` backend
storage, sRGB/full-range bounded semantics and `ForceOpaque` alpha where the
shader writes alpha one. This is the per-effect SDR fallback for effects whose
public HDR contract is unresolved. The route is still effect-owned, so the
dispatcher can report the exact effect ID and auxiliary resource boundary.

CAS exposes two declared options through the existing integer-choice parameter
syntax (`hdrFormat`):

| Option | Route | Evidence/profile | Runtime condition |
|---|---|---|---|
| `0` | `R8G8B8A8_UNORM` | SDR/full-range/sRGB, `SDRCompatible` | Default and fully wired with current compiler |
| `1` | `R16G16B16A16_FLOAT` | linear/scene-linear, `ConditionalFP16`, reference implementation evidence | Experimental selection; current effect compiler still emits the CAS main surface as R8, so `EffectDrawer` format matching keeps this route out of the direct path until a CAS-specific FP16 compile variant is supplied |

This preserves the distinction between a documented backend option and a
verified HDR-native path. CAS output alpha remains forced opaque in both route
descriptions, matching the HLSL writes.

## Minimal core wiring

`Renderer::_BuildEffects` preselects the effect-local route before texture
allocation, and `Renderer::_UpdateHdrEffectBoundaryContexts` refreshes it when
parameters change. `EffectDrawer` uses the selected route's input/output
format for HDR-mode working surfaces, which makes CAS option `1` an actual
FP16 surface path while preserving the existing R8 fallback for option `0`.
The shared dispatcher and conversion helpers retain their existing behavior;
the core wiring is limited to these two call sites and the provider registration.
