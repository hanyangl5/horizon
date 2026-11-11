#pragma once

#include "Core/IContainer.h"
#include "SceneView.h"

class GpuDrawList;

class RayTracingScene
{
public:
    explicit RayTracingScene(hz::RenderContext& context): context(context) {}
    void                                update(hz::CommandList& commands, const SceneView& view, const GpuDrawList& draws);
    const hz::GPUAccelerationStructure& getSceneTLAS() const { return tlas; }

private:
    hz::RenderContext&                      context;
    hz::Array<hz::GPUAccelerationStructure> blas;
    hz::GPUAccelerationStructure            tlas;
    uint64_t                                geometryRevision = 0;
};
