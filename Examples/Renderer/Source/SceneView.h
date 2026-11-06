#pragma once

#include "Scene/GpuScene.h"

// Scene inputs for one frame. The scene is frozen between GpuScene::beginFrame and endFrame;
// frameSlot selects per-frame resources and rotates with the frames in flight.
struct SceneView
{
    const hz::RenderScene&  scene;
    const hz::GpuScene&     gpu;
    const hz::AssetManager& assets;
    uint32_t                frameSlot = 0;
};
