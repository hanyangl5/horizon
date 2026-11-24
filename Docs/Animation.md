# Skeletal Animation

Skinned glTF characters cook into ozz skeletons and clips, play through the World and deform on the GPU.

## Cooking

- Skins whose joints hang below the same root nodes form one skeleton file (`<scene>.skeletonN.ozz`). Each skin stores
  the skeleton joint each of its joints follows, the inverse bind matrices, and the bind-space bounds of the vertices
  each joint influences.
- Each glTF animation becomes one clip per skeleton it animates (`<scene>.animationN.ozz`). Linear keys are kept, step
  keys are held until the next key, and cubic splines keep their key values without tangents. Channels on nodes
  outside a skeleton and morph weights are ignored with a warning.
- Skeleton nodes do not become World entities. Each skeleton becomes one node below the roots' parent, carrying the
  skeleton and its first clip, and every skinned mesh attaches to it with an identity transform. Non-joint nodes below
  joints keep their bind pose and do not follow the animation.
- Skeletons and clips have persistent AssetIDs in the source sidecar. A clip depends on its skeleton in the catalog.

## World

| Component | Meaning |
| --- | --- |
| `Skeleton` | The entity's world transform places the skeleton's model space. |
| `AnimationPlayer` | Clip, time, speed, loop and playing state for the `Skeleton` on the same entity. |
| `SkinnedMesh` | The `MeshRenderer` on the same entity is deformed by a skin of the `Skeleton` on the parent entity. |

`AnimationSystem` retains the skeleton and clip of every `Skeleton` entity, advances its player and writes the time
back to World. A clip that does not loop stops at its end. Call `Update` after the asset manager update and before
the frame's `RenderScene` update.

`SkinningCore` evaluates one skeleton pose: it samples and blends weighted clip layers, converts the pose to model space
and writes per skin the skinning matrices and the model-space bounds. It is usable without World, allocates its working
memory at construction and keeps ozz types out of the public headers.

## Rendering

- `RenderScene`, given the animation system, gathers the skinning matrices of every posed skinned instance each update
  and bounds the instance by the posed joint boxes. A skin whose skeleton has no pose renders the mesh in bind pose.
- `GpuSkinning` writes the matrices into a per-frame palette ring and keeps, per skinned instance, persistent streams
  covering the vertices its mesh references. The renderer's `Skinning.slang` pass skins positions and normals into
  them and copies texture coordinates; draws, shadows and ray tracing then read the skinned streams.
- Skinned bottom-level acceleration structures are refit every frame.

## Limits

- Motion vectors do not include skinned deformation, only the instance transform.
- Up to four joint influences per vertex, with 16-bit joint indices.
- glTF morph targets, node (non-skeleton) animation and additive clips are not supported.
