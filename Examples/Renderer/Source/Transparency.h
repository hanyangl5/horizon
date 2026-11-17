#pragma once

#include "DeferredLighting.h"
#include "GpuDrawList.h"

// Per-pixel linked-list OIT; see ../Cookbook-LICENSE.txt.
class Transparency
{
public:
    explicit Transparency(hz::RenderContext& Context, hz::Format DepthFormat = hz::Format::D24_UNORM_S8_UINT);
    void Resize(uint32_t Width, uint32_t Height, uint32_t FragmentsPerPixel = 4);

    const hz::GPUTexture& Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth, const SceneView& View,
                                  const GpuDrawList& Draws, const LightingResources& Lighting, const hz::GPUTexture& OpaqueColor);
    // uint4: attempted fragments, allocation overflow, pixels exceeding the 64-fragment resolve limit, reserved.
    const hz::GPUBuffer&  GetStatistics() const { return Counters; }
    const hz::GPUTexture& GetColor() const { return Color; }

    float OpacityBoost = 0;
    bool  ShowHeatmap = false;

private:
    hz::RenderContext&               Context;
    hz::GPUPipeline                  ClearPipeline;
    hz::GPUPipeline                  GatherPipeline;
    hz::GPUPipeline                  ResolvePipeline;
    hz::GPUSampler                   SurfaceSampler;
    hz::GPUTexture                   Heads;
    hz::GPUTexture                   Color;
    hz::GPUBuffer                    Nodes;
    hz::GPUBuffer                    Counters;
    hz::Array<const hz::GPUTexture*> Sampled;
    hz::Array<const hz::GPUBuffer*>  Buffers;
    uint32_t                         Width = 0;
    uint32_t                         Height = 0;
    uint32_t                         MaxNodes = 0;
};
