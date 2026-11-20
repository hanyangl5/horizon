#pragma once

#include "Core/IContainer.h"
#include "Scene/SceneView.h"

namespace hz
{
class GpuDrawList;
}

class RayTracingScene
{
public:
    explicit RayTracingScene(hz::RenderContext& Context): Context(Context) {}
    void                                Update(hz::CommandList& Commands, const hz::SceneView& View, const hz::GpuDrawList& Draws);
    const hz::GPUAccelerationStructure& GetSceneTLAS() const { return TopLevelAS; }

private:
    hz::RenderContext&                      Context;
    hz::Array<hz::GPUAccelerationStructure> BottomLevelAS;
    // Bottom levels built from skinned streams, refit every frame after the skinning pass.
    hz::Array<uint32_t>                     SkinnedBottomLevels;
    hz::GPUAccelerationStructure            TopLevelAS;
    uint64_t                                GeometryRevision = 0;
};
