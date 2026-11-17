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
    // Occlusion and Scratch share this desc at the viewport size.
    static constexpr hz::Format TargetFormat = hz::Format::R8G8B8A8_UNORM;

    explicit ScreenSpaceAO(hz::RenderContext& Context);
    bool Load();

    // Writes the result to Occlusion; each blur pass goes through Scratch and back.
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth, const hz::GPUSampler& Sampler,
                 const hz::GPUTexture& Occlusion, const hz::GPUTexture& Scratch);

    ScreenSpaceAOSettings Settings;

private:
    hz::RenderContext& Context;
    hz::GPUPipeline    AOPipeline;
    hz::GPUPipeline    BlurPipeline;
    hz::GPUSampler     RotationSampler;
    hz::GPUTexture     Rotations;
};
