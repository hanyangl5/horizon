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

Set `kSceneDirectory` and `kSceneSource` in `Source/Renderer.h` to select the model.
The default uses the external `Assets/Bistro` link. With `HORIZON_BUILD_TOOLS=ON`,
Renderer cooks missing or outdated assets before loading; unchanged outputs are
reused. Cooked files live alongside the executable in `RendererAssets`. Image cooking
uses DirectXTex to produce DDS textures. With tools disabled, the cooked catalog and
its assets must already exist.

Geometry uses manual vertex pulling and Slang shaders. Materials resolve texture
IDs through the GPU asset tables. Lighting reads directional lights from
World; the sample creates a sun and a controllable camera positioned from scene
bounds. Motion vectors use the previously submitted object and camera transforms.
The preview supports normal, metallic/roughness and emissive maps. MASK uses the
material alpha cutoff; BLEND currently uses a 0.1 cutoff. Shadows, environment
lighting and other light shapes are not shaded yet.

- WASD: move; Q/E: descend/ascend.
- Hold the right mouse button: look around.
- Hold left Shift: move four times faster.
- R: reset the camera; Escape: release the mouse.

Tracy shows CPU zones for Update, frame recording, scene extraction and GpuScene
uploads, plus GPU timings for each pass. Only Debug builds enable GPU-based
validation, which slows GPU work enough to distort those timings.

Up to three frames are in flight; Draw blocks on the oldest one instead of skipping.
In a window, present follows the display refresh, so GPU idle time between frames
is expected unless a frame takes longer than the refresh interval.

Losing window focus releases the mouse and stops movement. Resize rebuilds render
targets and resets render history. Shutdown waits for GPU work, then releases the
render passes and GpuScene, RenderScene, World, asset manager/loaders and render context in order.
