#pragma once

#include "Graphics/RenderContext.h"

struct ScreenSpaceAOSettings
{
    uint32_t blurPasses = 1;
    float    radius = 0.01f;
    // The deferred pass multiplies this into indirect light directly, so an unoccluded sample must stay 1.
    float    attenuationScale = 1.0f;
    float    distanceScale = 1.7f;
    float    depthThreshold = 30.0f;
};

// Adapted from Cookbook Renderer SSAOPass; see ../Cookbook-LICENSE.txt.
// Produces the ambient-occlusion image the deferred pass multiplies into indirect light.
class ScreenSpaceAO
{
public:
    explicit ScreenSpaceAO(hz::RenderContext& context);
    bool resize(uint32_t width, uint32_t height);

    const hz::GPUTexture& execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& depth,
                                  const hz::GPUSampler& sampler);
    const hz::GPUTexture& getOcclusion() const;

    ScreenSpaceAOSettings settings;

private:
    hz::RenderContext& context;
    hz::GPUPipeline    aoPipeline;
    hz::GPUPipeline    blurPipeline;
    hz::GPUSampler     rotationSampler;
    hz::GPUTexture     rotations;
    hz::GPUTexture     occlusion;
    hz::GPUTexture     blur[2];
    uint32_t           width = 0;
    uint32_t           height = 0;
    bool               blurred = false;
};
