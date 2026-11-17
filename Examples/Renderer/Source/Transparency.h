#pragma once

#include "LightingTypes.h"
#include "Scene/GpuDrawList.h"

// Viewport-sized resources the OIT pass works in. Nodes holds 16 bytes per fragment.
struct TransparencyTargets
{
    const hz::GPUTexture* Heads = nullptr;
    const hz::GPUTexture* Color = nullptr;
    const hz::GPUBuffer*  Nodes = nullptr;
    // uint4: attempted fragments, allocation overflow, pixels exceeding the 64-fragment resolve limit, reserved.
    const hz::GPUBuffer*  Counters = nullptr;
};

// Per-pixel linked-list OIT; see ../Cookbook-LICENSE.txt.
class Transparency
{
public:
    static constexpr uint32_t   NodeBytes = 16;
    static constexpr hz::Format HeadsFormat = hz::Format::R32_UINT;
    static constexpr hz::Format ColorFormat = hz::Format::R32G32B32A32_SFLOAT;

    explicit Transparency(hz::RenderContext& Context, hz::Format DepthFormat = hz::Format::D24_UNORM_S8_UINT);

    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth, const hz::SceneView& View,
                 const hz::GpuDrawList& Draws, const LightingResources& Lighting, const hz::GPUTexture& OpaqueColor,
                 const TransparencyTargets& Targets);

    float OpacityBoost = 0;
    bool  ShowHeatmap = false;

private:
    hz::GPUPipeline                  ClearPipeline;
    hz::GPUPipeline                  GatherPipeline;
    hz::GPUPipeline                  ResolvePipeline;
    hz::GPUSampler                   SurfaceSampler;
    hz::Array<const hz::GPUTexture*> Sampled;
    hz::Array<const hz::GPUBuffer*>  Buffers;
};
