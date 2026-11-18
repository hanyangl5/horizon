# ReSTIR DI

`Examples/Renderer` lights the GBuffer with ReSTIR DI (Bitterli et al. 2020) for point and spot lights, plus the
analytic sun and image-based light of the Cookbook deferred pass. The pass lives in `RestirDI.cpp` and `RestirDI.slang`.

## Frame

1. **Candidates** (`CSCandidates`): each pixel draws `InitialCandidates` lights uniformly from the frame's light table
   and keeps one by resampled importance sampling with the unshadowed target `luminance(f * Le * cos)`. It then merges the
   previous frame's reservoir at the motion-reprojected pixel, clamped to `MaxHistory * InitialCandidates` samples, and
   stores the result as this frame's history.
2. **Shade** (`CSShade`): merges `SpatialSamples` neighbouring reservoirs within `SpatialRadius` pixels, traces one
   inline shadow ray toward the selected light with the same alpha test as the RT shadows, and composes the lit color.

Spatial reuse feeds shading only and never the history, so a bright sample cannot spread through both reuse passes
frame after frame. Reuse is the biased variant: neighbours are reweighted by the target at the receiver without
visibility, and history or neighbours are rejected by a 10% relative view-depth difference, a different material ID
or, for neighbours, a normal difference above about 25 degrees.

The sun keeps its analytic evaluation with the denoised RT or shadow-map visibility. Its illuminance would win almost
every unshadowed resampling and leave shadowed pixels without a sample.

## Data

| Item | Layout |
| --- | --- |
| Light table | `RestirDILight`, 64 bytes, up to 256 point and spot lights rebuilt from `RenderScene` every frame, one buffer per frame in flight |
| Reservoir | `RestirDIReservoir`, 16 bytes: light index and receiver material ID, `W`, `M`, receiver view depth |
| History | Two reservoir buffers that alternate between frames; resize and settings changes restart it |

The table holds delta lights, so the light index identifies the sample. Area lights need a sample position in the
reservoir.

## Settings

`RestirDI::Settings` controls the candidate count, temporal and spatial reuse and the history clamp. `Reference` sums
every local light with its own shadow ray; it is the ground truth the reservoir path is compared against.

The acceleration structures are refit by their own frame-graph pass, so the shadow rays work when the RT effects are
disabled. Without ray tracing support local lights are unshadowed.

## Limits

- Only point and spot lights are resampled. Rect, disk, sphere, cylinder, dome and mesh lights are skipped.
- Reuse is biased; there is no visibility in the target and no pairwise MIS.
- The result is not denoised; temporal reuse keeps the noise low while the camera is static.
