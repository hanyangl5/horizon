# Renderer

Loads BistroExterior using SceneManager in the Runtime Scene layer: the asset cooker produces GeometryTF geometry
and a scene manifest, and the runtime loads geometry, materials and DDS textures
through `SceneManager::requestFromGltf`. glTF node transforms, primitive material assignments
and the selected scene's perspective camera are preserved in cooked geometry metadata.

```powershell
cmake -S . -B build
cmake --build build --config Release --target Renderer
./build/Release/Renderer.exe
```

Set the model path manually through `kSceneSource` in `Renderer.cpp`. The sample
derives the texture directory and source filename from it. With `HORIZON_BUILD_TOOLS=ON`
(the default), Renderer links `AssetPipeline.lib` and registers `ensureSceneGltfCooked`
with SceneManager. A glTF request synchronously checks the source, dependencies and
cooking settings; missing or outdated assets are cooked before asynchronous GPU loading.
An unchanged cache is reused without rewriting outputs. Generated files
live in `build/RendererAssets`; textures are read from the source glTF directory.
The source model is external and is not copied into the repository. CMake only
configures the cooked output directory; building Renderer does not cook assets.
The optional cooker parses glTF; SceneManager loads its cooked manifest and geometry.
Only glTF scenes with already supported DDS/KTX textures are supported; this does not
add image compression. Cook requests must run serially on the main thread.

With tools disabled, Renderer can load existing cooked assets but cannot generate or
validate their content hashes. The standalone command remains available when tools are built:

```powershell
cmake --build build --config Release --target AssetPipelineCmd
./build/Release/AssetPipelineCmd.exe -pgltf --input-file "D:/Codes/models/Bistro_v5_2/BistroExterior.gltf" --output "build/RendererAssets"
```

The initial preview uses direct lighting plus ambient light, base color, BC5 normal,
metallic/roughness and emissive maps. It has no shadows or environment lighting yet;
MASK materials use their alpha cutoff, and BLEND materials currently use a 0.1 cutoff
instead of sorted transparency. The FPS camera starts at the authored glTF position
and direction when available, with world Y as up.

- WASD: move; Q/E: descend/ascend.
- Hold the right mouse button: look around.
- Hold left Shift: move four times faster.
- R: reset the camera; Escape: release the mouse.

Losing window focus releases the mouse and stops movement.

SceneManager adopts uploaded buffers and textures into scene-owned `GPUBuffer` and
`GPUTexture` objects. `SceneGeometry` exposes vertex/index buffers for direct command
binding; material and texture getters return const GPU resource pointers.
Scene loading uses standalone geometry buffers; shared `GeometryBuffer`
requests are rejected. Wait for GPU work before releasing scenes, and destroy the
manager before its render context. Shaders and pipelines have separate owners.
