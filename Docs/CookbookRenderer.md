# Cookbook Renderer

The reference pipeline is `3D-Graphics-Rendering-Cookbook-Second-Edition/Renderer`.
The port reads the World through RenderScene and GpuScene, and uses bindless D3D12, Slang and resource-owning
pass classes. No application code issues native barriers.

`RenderPasses` registers every pass once on a frame graph
([FrameGraph.h](../Source/Runtime/Graphics/Public/Graphics/FrameGraph.h)). Each pass declares the textures and buffers
it reads and writes; the graph culls passes whose results nothing reads, sizes targets relative to the viewport, lets
transient targets with disjoint lifetimes share memory, and keeps two copies of history targets such as the denoiser
accumulation. Effects are gated with `SetFrameGraphPassEnabled`, and an optional read resolves to null when its writer
is disabled. Resizing recreates viewport-sized targets after outstanding GPU work completes and invalidates history;
pass objects, pipelines, environment assets and acceleration structures survive. Barriers stay with `CommandList`,
which transitions each resource a command declares in its `Dependencies`, so the graph never tracks resource states.

`GpuDrawList` (Scene runtime) flattens the RenderScene into per-draw data and owns the indirect lists. Lists are
registered with a `DrawListFilter` (accepted alpha modes, required material tag bits and whether frustum culling
applies), similar to an SRP renderer list; the opaque, transparent and shadow-caster lists are the defaults. The
sample's culling pass fills every list in one dispatch, so a GPU-driven culling pipeline can replace it without
changing the drawing passes.

Constant blocks are named structs that mirror their shader block and assert their size.

Shader alignment distinguishes the reference algorithm from engine bindings.
RT shadow/AO restores the original inline control flow, sampling helpers and
experimental first-hit retracing; the surface normal is decoded in float like the
deferred pass, while the occlusion accumulation keeps the reference half arithmetic.
The fragment pass is dispatched as a compute kernel over the same pixel centers and
writes the same shadow/AO channels. HLSL requires InstanceID queries
instead of Vulkan CustomIndex queries. The shadow/AO denoiser departs from the
reference kernels; see the Denoising row below. RT reflection restores the
original entry names and FORCE_OPAQUE ray flag; the added AnyHit path and extra hit-roughness clamp are removed. Consequently,
reflection treats masked geometry as opaque, as in the reference; raster geometry
and RT shadows still perform their own mask tests.

Normal mapping, PBR, frustum tests, SSAO, transparent shading/sorting,
exposure adaptation and tone mapping restore the reference formulas and control
flow. Bloom keeps the reference exposure-relative threshold but intentionally
replaces the diamond filter with a half-resolution box/tent chain.
Existing Horizon entry names are retained. GBuffer formats, vertex/index
storage, bindless heap access, colored lights, Specular and HDR output remain
engine adaptations. Shadow maps retain per-texel MASK tests for Bistro foliage;
disabled reflection retains its invalid-TLAS output-clear guard. BC5 normal maps
reconstruct their missing Z channel before entering the reference RGB normal functions. These differences
and the input assets prevent text-level or pixel-level identity.

| Feature | Implementation and verification |
| --- | --- |
| GPU culling | Six planes and eight frustum corners against world AABBs. GPU readback checks perspective projection, camera translation/rotation, visibility and triangle counts, plus off-camera shadow casters. |
| Indirect draw | GPU-generated opaque, transparent and shadow commands. D3D12 root constants provide draw IDs; manual index/vertex pulling supports World mesh buffers. Raster readback checks distinct materials and the last of three uint16 indices. |
| Shadow map | Default 2048x2048 D16, masked geometry, hardware comparison filtering. The shared geometry shader passes opaque/masked depth tests; the light projection quantizes its light-space extents and snaps their centre to the texel grid instead of following the scene bounds exactly, so the map stays stable while the bounds move. D3D12 uses integer constant depth bias 1 for the reference Vulkan value 1.1. |
| RT shadow/AO | Actual inline ray queries with the original blue-noise texels, alpha candidates/retracing and cosine AO. Production-shader tests verify opaque occlusion and alpha-cutoff rejection. |
| Resolution | RT shadow/AO, reflection and the denoiser run at half the viewport width and height; each texel traces from one full-resolution GBuffer pixel. Deferred lighting upsamples with bilinear weights scaled by relative view-distance difference and falls back to the closest-depth texel. |
| Denoising | Camera reprojection with per-tap relative view-depth rejection, per-pixel history length and variance clipping, then two edge-aware 5x5 a-trous iterations weighted by normal and plane distance. Mirror reflections, which trace one deterministic ray per texel, are only accumulated temporally with the same reprojection and a YCoCg clip to the current 3x3 neighbourhood; they get no spatial filter. Settings/asset/resize changes reset history; object motion is not compensated. |
| RT reflection | Actual DXIL raygen/miss/closest-hit, DXR state object and shader table. Hit/miss, TLAS update and subsequent compute readback pass; Bistro reflection is nonzero and finite. Receiver Fresnel is applied once. |
| Lighting | GGX, correlated Smith, Schlick, diffuse/specular IBL and BRDF LUT. A production-shader highlight test preserves about 229501 in RGBA32F and matches the analytic BRDF value; the 4-bit specular default round-trips 0.5, so the deferred and ray traced paths share F0 0.04. |
| Skybox | Original prefiltered cubemap, irradiance and LUT, with the same cube orientation used by IBL and reflection misses. |
| SSAO | Original eight rotated samples and depth-aware blur; optional, consumed by the deferred pass as indirect-light occlusion and skipped when RT AO is enabled. Both entry points compile in the production-shader test. |
| OIT | PPLL half4 colors, depth sorting and 64-layer resolve. Transparent fragments take direct-light visibility from the shadow map. Production-shader readback verifies draw-order independence, opaque occlusion and allocation overflow. |
| Bloom | Reference exposure-relative soft-knee threshold on a half-resolution six-level chain: each level is a 2x2 box downsample and a 3x3 tent upsample accumulates `down + tent(up)` back to level 0. This deliberately replaces the reference diamond filter. GPU box, tent and constant-preservation checks pass. |
| Adaptation | Log luminance, reduction and exponential ping-pong adaptation. Ten independent R16F level textures replace mip views; GPU initialization, log-mean and exposure-step checks pass. |
| Tone mapping | None, Reinhard, Uchimura and Khronos PBR, plus Horizon SDR/HDR10/scRGB output. All curves and HDR transfer functions have GPU numerical checks; physical SDR and HDR10 Renderer paths execute. |

The order is shadow map, geometry, optional SSAO, RT shadow/AO and denoising, RT
reflection, deferred lighting, sky, transparency/resolve, then HDR postprocessing.
Main lit targets use RGBA32F, matching the reference range. PPLL nodes retain the
reference half4 storage and its 65504 color limit.

Directional lights use perpendicular illuminance in lux. The Bistro sun is set to
100,000 lux. Local lights specify intensity units on their component. Sky, indirect lighting, transparency and reflection misses
share the environment intensity in cd/m² per unit of HDR radiance.

Agility SDK 722 fixes the earlier SDKLayers DXIL-library instrumentation error.
DeferredLighting binds its graphics PSO before attachment transitions to avoid a
GBV compatibility problem after DispatchRays; this is not a general D3D12 barrier
ordering requirement. No dummy PSO, extra public option or manual barrier is used.

Original environment files are read through the local `Assets/Cookbook` link to
the reference `data` directory. Rotation and blue-noise DDS conversions preserve
the original texels. Adapted code retains the MIT notice in
`Examples/Renderer/Cookbook-LICENSE.txt`.

The current Bistro input is exterior-only FBX2glTF. Its lost cutout flags and two
missing opacity maps are repaired by the sample's preparation script; see the
[Renderer setup](../Examples/Renderer/README.md). GBuffer MASK retains the reference
4x4 dither and derivative-adjusted cutoff, with GPU coverage checks. The reference
OBJ cache still includes the interior, different scale and material heuristics,
so the two inputs cannot establish pixel equality.

Verification uses RTX 3080, real shader compilation and GPU readback. The bounded
Renderer checks the complete pipeline with GBV at 1920x1080, World mesh/camera
updates, 1x1 and restored sizes, filter/fallback changes and shutdown, with zero
D3D12 errors. MSVC and Clang Debug/Release builds and the original 115 Scene tests pass;
the other 65 Core/RHI/Resources/Graphics/Smoke checks also pass in MSVC Debug.
The additional GBuffer MASK test checks opaque/zero/full alpha, 8/16 near dither
coverage and 7/16 distant coverage on the GPU. The repaired Bistro has 140 MASK
draws; its foliage view, ray tracing, resize and shutdown check has zero D3D12
validation errors. Two restored opacity textures were cooked with OpenMP BC7;
the cooker logs texture dimensions, mip progress and elapsed time.
Runtime test logs and captures remain in ignored `build/`; no test-only startup
options, environment variables or CMake fixtures are added to the application.
