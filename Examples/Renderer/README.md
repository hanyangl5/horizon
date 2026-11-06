# Renderer

Loads Bistro through AssetCatalog and AssetManager, instantiates its node hierarchy
in World, and renders it through GpuScene, which keeps one persistent GPU copy of the
scene and asset tables and uploads only changes each frame. Meshes, materials and
textures are shared through typed asset records; released assets and replaced buffers
are destroyed only after the GPU work that may use them completes.

```powershell
cmake -S . -B build
cmake --build build --config Release --target Renderer
./build/Release/Renderer.exe
```

The environment and noise textures are local assets and are not versioned. Prepare
them from the Cookbook checkout before running (image conversion requires Pillow):

```powershell
python Scripts/PrepareRendererAssets.py ../3D-Graphics-Rendering-Cookbook-Second-Edition
New-Item -ItemType Junction -Path Assets/Cookbook -Target (Resolve-Path ../3D-Graphics-Rendering-Cookbook-Second-Edition/data)
```

Set `kSceneDirectory` and `kSceneSource` in `Source/Renderer.h` to select the model.
The default uses the external `Assets/Bistro` link. With `HORIZON_BUILD_TOOLS=ON`,
Renderer cooks missing or outdated assets before loading; unchanged outputs are
reused. Cooked files live alongside the executable in `RendererAssets`. Image cooking
uses DirectXTex to produce DDS textures, with OpenMP compression when available
and logs for mip progress and elapsed time. With tools disabled, the cooked
catalog and its assets must already exist.

The FBX2glTF Bistro export loses cutout material flags. Prepare it once before
cooking (the optional reference path also restores the two missing opacity maps;
that option requires Pillow):

```powershell
python Scripts/PrepareBistro.py Assets/Bistro/BistroExterior.gltf --reference ../3D-Graphics-Rendering-Cookbook-Second-Edition
```

This updates only known Bistro cutout materials and two image references, saves a
`.gltf.before-mask-fix` backup, and leaves the original DDS textures intact.
Omit `--reference` to repair only the eight materials whose DDS already has alpha.
The next launch recooks changed assets automatically.

Geometry uses manual vertex pulling and Slang shaders. Materials resolve texture
IDs through the GPU asset tables. Lighting reads directional lights from
World; the sample creates a sun and a controllable camera positioned from scene
bounds. Motion vectors use the previously submitted object and camera transforms.
The preview reconstructs Z from Bistro's BC5 normal maps and supports metallic/roughness
and emissive maps. MASK follows
the reference shader's alpha dither and derivative-adjusted cutoff; BLEND uses
PPLL transparency. Directional shadows, ray-traced shadow/AO/reflection, IBL and
HDR postprocessing are described in [CookbookRenderer](../../Docs/CookbookRenderer.md).

- WASD: move; Q/E: descend/ascend.
- Hold the right mouse button: look around.
- Hold left Shift: move four times faster.
- R: reset the camera; Escape: release the mouse.

Tracy shows CPU zones for Update, frame recording, scene extraction, GpuScene
uploads and each pass, plus GPU timings for each pass. Only Debug builds enable
GPU-based validation, which slows GPU work enough to distort those timings.

Up to three frames are in flight; Draw blocks on the oldest one instead of skipping.
With VSync on, or in a window, present follows the display refresh, so GPU idle time
between frames is expected unless a frame takes longer than the refresh interval.

Losing window focus releases the mouse and stops movement. Each pass has a single
`resize()` for size-dependent targets; fixed pipelines, environment textures,
blue noise and acceleration structures remain alive. Resize resets render history.
Shutdown waits for GPU work, then releases the render passes and GpuScene,
RenderScene, World, asset manager/loaders and render context in order.
