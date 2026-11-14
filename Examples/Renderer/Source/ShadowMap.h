#pragma once
#include "GpuDrawList.h"
#include "DeferredLighting.h"

class ShadowMap
{
public:
    // One map covers the whole scene, so the resolution trades 8 MB of D16 and shadow fill rate for texel density.
    explicit ShadowMap(hz::RenderContext& context, uint32_t newResolution = 2048);
    void                  execute(hz::CommandList&, const SceneView&, const GpuDrawList&);
    const hz::GPUTexture& getTexture() const { return depth; }
    const hz::GPUSampler& getSampler() const { return sampler; }
    const hz::GPUBuffer&  getLightingFrame(uint32_t backing) const { return lightingFrames[backing]; }
    float                 luminousEfficacy = 100.0f; // lm/W; explicit Cookbook daylight-white calibration.
    LightingFrameData     data;

private:
    hz::RenderContext&               context;
    uint32_t                         resolution;
    hz::GPUPipeline                  pipeline;
    hz::GPUTexture                   depth;
    hz::GPUSampler                   sampler;
    hz::GPUSampler                   surfaceSampler;
    hz::FixedArray<hz::GPUBuffer, 3> shadowFrames;
    hz::FixedArray<hz::GPUBuffer, 3> lightingFrames;
};
