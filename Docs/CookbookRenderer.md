# Cookbook Renderer

The reference pipeline is `3D-Graphics-Rendering-Cookbook-Second-Edition/Renderer`.
The port reads the World through RenderScene and GpuScene, and uses bindless D3D12, Slang and resource-owning
pass classes. The application still calls `update()` and `execute()`; dependencies,
indirect-argument transitions and acceleration-structure synchronization stay in
Graphics/RHI. No application code issues native barriers.

Passes expose `resize()` rather than paired load/unload hooks. The application
waits for outstanding GPU work before resizing. Pass objects, fixed pipelines,
environment assets, blue noise and acceleration structures survive; old size
targets are released by RAII and temporal history is invalidated. Tone mapping
rebuilds its PSO only when the output format changes.

Shader alignment distinguishes the reference algorithm from engine bindings.
RT shadow/AO restores the original inline control flow, half arithmetic, sampling
helpers and experimental first-hit retracing. The fragment pass is dispatched as
a compute kernel over the same pixel centers and writes the same shadow/AO channels. HLSL requires InstanceID queries
instead of Vulkan CustomIndex queries. Temporal, spatial and atrous denoising use
the original kernel files and math; history invalidation adds an initialization
guard. RT reflection restores the original entry names and FORCE_OPAQUE ray flag;
the added AnyHit path and extra hit-roughness clamp are removed. Consequently,
reflection treats masked geometry as opaque, as in the reference; raster geometry
and RT shadows still perform their own mask tests.

Normal mapping, PBR, frustum tests, SSAO, transparent shading/sorting, Bloom,
exposure adaptation and tone mapping restore the reference formulas and control
flow. Existing Horizon entry names are retained. GBuffer formats, vertex/index
storage, bindless heap access, colored lights, Specular and HDR output remain
engine adaptations. Shadow maps retain per-texel MASK tests for Bistro foliage;
disabled reflection retains its invalid-TLAS output-clear guard. BC5 normal maps
reconstruct their missing Z channel before entering the reference RGB normal functions. These differences
and the input assets prevent text-level or pixel-level identity.

| Feature | Implementation and verification |
| --- | --- |
| GPU culling | Six planes and eight frustum corners against world AABBs. GPU readback checks perspective projection, camera translation/rotation, visibility and triangle counts, plus off-camera shadow casters. |
| Indirect draw | GPU-generated opaque, transparent and shadow commands. D3D12 root constants provide draw IDs; manual index/vertex pulling supports World mesh buffers. Raster readback checks distinct materials and the last of three uint16 indices. |
| Shadow map | 512x512 D16, masked geometry, hardware comparison filtering. The shared geometry shader passes opaque/masked depth tests; light projection and PCF follow the reference. D3D12 uses integer constant depth bias 1 for the reference Vulkan value 1.1. |
| RT shadow/AO | Actual inline ray queries with the original blue-noise texels, alpha candidates/retracing and cosine AO. Production-shader tests verify opaque occlusion and alpha-cutoff rejection. |
| Denoising | Camera reprojection, temporal depth rejection, Gaussian or atrous filtering. Settings/asset/resize changes reset history; no extra motion compensation beyond the reference. Both filter paths execute in the bounded Renderer check. |
| RT reflection | Actual DXIL raygen/miss/closest-hit, DXR state object and shader table. Hit/miss, TLAS update and subsequent compute readback pass; Bistro reflection is nonzero and finite. Receiver Fresnel is applied once. |
| Lighting | GGX, correlated Smith, Schlick, diffuse/specular IBL and BRDF LUT. A production-shader highlight test preserves about 242679 in RGBA32F and matches the analytic BRDF value. |
| Skybox | Original prefiltered cubemap, irradiance and LUT, with the same cube orientation used by IBL and reflection misses. |
| SSAO | Original eight rotated samples and depth-aware blur; optional and not applied on top of RT AO. Verified through the Renderer fallback path. |
| OIT | PPLL half4 colors, depth sorting and 64-layer resolve. Production-shader readback verifies draw-order independence, opaque occlusion and allocation overflow. |
| Bloom | Original diamond filter, alternating 2.5/1.5 radii and defaults. GPU impulse-response and energy checks pass. |
| Adaptation | Log luminance, reduction and exponential ping-pong adaptation. Ten independent R16F level textures replace mip views; GPU initialization, log-mean and exposure-step checks pass. |
| Tone mapping | None, Reinhard, Uchimura and Khronos PBR, plus Horizon SDR/HDR10/scRGB output. All curves and HDR transfer functions have GPU numerical checks; physical SDR and HDR10 Renderer paths execute. |

The order is shadow map, geometry, RT shadow/AO and denoising, RT reflection,
deferred lighting, sky, transparency/resolve, optional SSAO, then HDR postprocessing.
Main lit targets use RGBA32F, matching the reference range. PPLL nodes retain the
reference half4 storage and its 65504 color limit.

The example explicitly chooses a 100 lm/W daylight-white calibration: a World
light of 1000 W/m^2 becomes 100000 lux at the renderer boundary. This is a white-
light assumption, not a general lux/watt conversion. Environment calibration is
1000 cd/m^2 per HDR unit and is shared by sky, lighting, OIT and reflection.

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
