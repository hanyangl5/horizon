#pragma once

#include "Core/IContainer.h"
#include "SceneView.h"

class GpuDrawList;

class RayTracingScene
{
public:
    explicit RayTracingScene(hz::RenderContext& Context): Context(Context) {}
    void                                Update(hz::CommandList& Commands, const SceneView& View, const GpuDrawList& Draws);
    const hz::GPUAccelerationStructure& GetSceneTLAS() const { return TopLevelAS; }

private:
    hz::RenderContext&                      Context;
    hz::Array<hz::GPUAccelerationStructure> BottomLevelAS;
    hz::GPUAccelerationStructure            TopLevelAS;
    uint64_t                                GeometryRevision = 0;
};
