#pragma once

#include "Graphics/RenderContext.h"

struct ScreenSpaceAOSettings
{
    uint32_t blurPasses = 1;
    float    radius = 0.01f;
    float    attenuationScale = 0.95f;
    float    distanceScale = 1.7f;
    float    depthThreshold = 30.0f;
    float    scale = 1.1f;
    float    bias = 0.1f;
};

// Adapted from Cookbook Renderer SSAOPass; see ../Cookbook-LICENSE.txt.
class ScreenSpaceAO
{
public:
    explicit ScreenSpaceAO(hz::RenderContext& context);
    bool resize(uint32_t width, uint32_t height);

    const hz::GPUTexture& execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& depth,
                                  const hz::GPUTexture& normal, const hz::GPUSampler& sampler);
    const hz::GPUTexture& combine(hz::CommandList& commands, const hz::GPUTexture& color, const hz::GPUSampler& sampler);
    const hz::GPUTexture& getOcclusion() const;

    ScreenSpaceAOSettings settings;

private:
    hz::RenderContext& context;
    hz::GPUPipeline    aoPipeline;
    hz::GPUPipeline    blurPipeline;
    hz::GPUPipeline    combinePipeline;
    hz::GPUSampler     rotationSampler;
    hz::GPUTexture     rotations;
    hz::GPUTexture     occlusion;
    hz::GPUTexture     blur[2];
    hz::GPUTexture     combined;
    uint32_t           width = 0;
    uint32_t           height = 0;
    bool               blurred = false;
};
