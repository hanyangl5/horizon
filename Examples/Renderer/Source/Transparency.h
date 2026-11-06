#pragma once

#include "DeferredLighting.h"
#include "GpuDrawList.h"

// Per-pixel linked-list OIT; see ../Cookbook-LICENSE.txt.
class Transparency
{
public:
    explicit Transparency(hz::RenderContext& context, hz::Format depthFormat = hz::Format::D24_UNORM_S8_UINT);
    void resize(uint32_t width, uint32_t height, uint32_t fragmentsPerPixel = 4);

    const hz::GPUTexture& execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& depth,
                                  const hz::SceneFrameSnapshot& scene, const GpuDrawList& draws, const LightingResources& lighting,
                                  const hz::GPUTexture& opaqueColor);
    // uint4: attempted fragments, allocation overflow, pixels exceeding the 64-fragment resolve limit, reserved.
    const hz::GPUBuffer&  getStatistics() const { return counters; }
    const hz::GPUTexture& getColor() const { return color; }

    float opacityBoost = 0;
    bool  showHeatmap = false;

private:
    hz::RenderContext&               context;
    hz::GPUPipeline                  clearPipeline;
    hz::GPUPipeline                  gatherPipeline;
    hz::GPUPipeline                  resolvePipeline;
    hz::GPUSampler                   surfaceSampler;
    hz::GPUTexture                   heads;
    hz::GPUTexture                   color;
    hz::GPUBuffer                    nodes;
    hz::GPUBuffer                    counters;
    hz::Array<const hz::GPUTexture*> sampled;
    hz::Array<const hz::GPUBuffer*>  buffers;
    uint32_t                         width = 0;
    uint32_t                         height = 0;
    uint32_t                         maxNodes = 0;
};
