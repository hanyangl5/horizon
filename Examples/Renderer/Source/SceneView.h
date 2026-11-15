#pragma once

#include "Scene/GpuScene.h"

// Scene inputs for one frame. The scene is frozen between GpuScene::beginFrame and endFrame;
// FrameIndex selects per-frame resources and rotates with the frames in flight.
struct SceneView
{
    const hz::RenderScene&  Scene;
    const hz::GpuScene&     GpuScene;
    const hz::AssetManager& Assets;
    uint32_t                FrameIndex = 0;
};
