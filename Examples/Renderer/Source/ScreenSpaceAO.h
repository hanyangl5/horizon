#pragma once

#include "Graphics/RenderContext.h"

struct ScreenSpaceAOSettings
{
    uint32_t BlurPasses = 1;
    float    Radius = 0.01f;
    // The deferred pass multiplies this into indirect light directly, so an unoccluded sample must stay 1.
    float    AttenuationScale = 1.0f;
    float    DistanceScale = 1.7f;
    float    DepthThreshold = 30.0f;
};

// Adapted from Cookbook Renderer SSAOPass; see ../Cookbook-LICENSE.txt.
// Produces the ambient-occlusion image the deferred pass multiplies into indirect light.
class ScreenSpaceAO
{
public:
    explicit ScreenSpaceAO(hz::RenderContext& Context);
    bool Resize(uint32_t NewWidth, uint32_t NewHeight);

    const hz::GPUTexture& Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                                  const hz::GPUSampler& Sampler);
    const hz::GPUTexture& GetOcclusion() const;

    ScreenSpaceAOSettings Settings;

private:
    hz::RenderContext& Context;
    hz::GPUPipeline    AOPipeline;
    hz::GPUPipeline    BlurPipeline;
    hz::GPUSampler     RotationSampler;
    hz::GPUTexture     Rotations;
    hz::GPUTexture     Occlusion;
    hz::GPUTexture     Blur[2];
    uint32_t           Width = 0;
    uint32_t           Height = 0;
    bool               Blurred = false;
};
