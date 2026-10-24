# Renderer

Loads Bistro through AssetCatalog and AssetRegistry, instantiates its node hierarchy
in World, and renders paired RenderScene/asset frame snapshots. Meshes, materials and
textures are shared through typed asset records; submitted snapshots retain their
resources until the GPU fence completes.

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
IDs through the snapshot's GPU tables. Lighting reads directional lights from
World; the sample creates a sun and a controllable camera positioned from scene
bounds. Motion vectors use the previously submitted object and camera transforms.
The preview supports normal, metallic/roughness and emissive maps. MASK uses the
material alpha cutoff; BLEND currently uses a 0.1 cutoff. Shadows, environment
lighting and other light shapes are not shaded yet.

- WASD: move; Q/E: descend/ascend.
- Hold the right mouse button: look around.
- Hold left Shift: move four times faster.
- R: reset the camera; Escape: release the mouse.

Losing window focus releases the mouse and stops movement. Resize rebuilds render
targets and resets render history. Shutdown waits for GPU work, then releases the
frame pool, RenderScene, World, asset registry/loaders and render context in order.
