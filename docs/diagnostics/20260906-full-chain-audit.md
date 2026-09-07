# Full-chain HDR/SDR audit

This audit treats the current research tree as a multi-boundary rendering
system. Each boundary is required to preserve an explicit resource and color
contract. A successful SDK call does not count as an image result.

## Boundary inventory

1. Profile/config: `enableHdrCompatibility` is persisted and reaches
   `ScalingOptions::IsHdrCompatibilityEnabled()`.
2. Capture: `FrameSourceBase` selects the capture surface and, when HDR is on,
   invokes `HdrCaptureProcessor` to produce canonical FP16.
3. Canonical handoff: `Renderer` creates an `HdrFrame` around the capture
   output and refreshes effect boundaries.
4. Effect boundary: `EffectDrawer` allocates route formats, converts input,
   executes every production pass, converts output, and returns canonical FP16.
5. Native boundary: native backends receive route-compatible resources and
   their own temporal/auxiliary contracts.
6. Publication: backend output enters shared presentation textures and
   pass-through reference textures.
7. Frontend: shared textures are copied to presenter resources and drawn to the
   swap chain or composition surface.
8. Export: screenshots/readback select a texture and encode it using the
   matching format contract.

## Current failure evidence

- Runtime logs show adapter dispatch and native evaluation, while no stage
  emits actual pixel statistics or a readback image.
- The research tree differs from the SDR baseline in the common drawer and
  native factory. HDR-disabled behavior therefore needs an explicit regression
  gate before HDR results are trusted.
- A pass-through/reference surface is a distinct publication branch. It must
  use the same output color contract as the processed branch.
- Route metadata currently names formats and profiles, while the running
  effect descriptor may still allocate an incompatible format internally. The
  bridge must compare both values at runtime.

## Acceptance gate for the real bridge

For every configured effect and both HDR states, the bridge must record:

- exact source and output `EffectDesc`;
- selected route and adapter profile;
- every texture's DXGI format, dimensions, bind flags and state;
- actual pass dispatch count;
- GPU readback statistics and saved image hash;
- native backend result when present;
- presentation texture statistics.

Any missing stage, empty/black readback, invalid finite count, unchanged hash,
or route/descriptor mismatch is a failed row. No aggregate pass result may
hide a failed effect row.
