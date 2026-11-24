#pragma once

#include "Scene/GpuScene.h"

namespace hz
{
// Scene inputs for one frame. The scene is frozen between GpuScene::BeginFrame and EndFrame;
// FrameIndex selects per-frame resources and rotates with the frames in flight.
class GpuSkinning;

struct SceneView
{
    const RenderScene&  Scene;
    const GpuScene&     GpuScene;
    const AssetManager& Assets;
    uint32_t            FrameIndex = 0;
    // Skinned instances draw and trace the streams it writes; without it they use their mesh streams.
    const GpuSkinning*  Skinning = nullptr;
};
} // namespace hz
