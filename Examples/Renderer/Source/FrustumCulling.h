#pragma once

#include "Scene/GpuDrawList.h"

// Fills every list of a GpuDrawList on the GPU: each draw passes a list's filter by alpha mode and material tags, and
// lists that ask for it also drop draws outside the camera frustum. Later culling stages (occlusion, meshlets) can run
// before it and narrow the same lists.
class FrustumCulling
{
public:
    explicit FrustumCulling(hz::RenderContext& Context);
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws, bool Culling = true);
    // uint4: visible draws and their triangles, reserved; written by the latest Execute for that frame index.
    const hz::GPUBuffer& GetStatistics(uint32_t Backing) const { return Statistics[Backing]; }

private:
    hz::GPUPipeline                  Pipeline;
    hz::GPUPipeline                  ClearPipeline;
    hz::FixedArray<hz::GPUBuffer, 3> Statistics;
};
