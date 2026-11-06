#pragma once
#include "GpuDrawList.h"
#include "DeferredLighting.h"

class ShadowMap
{
public:
    explicit ShadowMap(hz::RenderContext& context);
    void                  execute(hz::CommandList&, const hz::SceneFrameSnapshot&, const GpuDrawList&);
    const hz::GPUTexture& getTexture() const { return depth; }
    const hz::GPUSampler& getSampler() const { return sampler; }
    const hz::GPUBuffer&  getLightingFrame(uint32_t backing) const { return lightingFrames[backing]; }
    float                 luminousEfficacy = 100.0f; // lm/W; explicit Cookbook daylight-white calibration.
    LightingFrameData     data;

private:
    hz::RenderContext&               context;
    hz::GPUPipeline                  pipeline;
    hz::GPUTexture                   depth;
    hz::GPUSampler                   sampler;
    hz::GPUSampler                   surfaceSampler;
    hz::FixedArray<hz::GPUBuffer, 3> shadowFrames;
    hz::FixedArray<hz::GPUBuffer, 3> lightingFrames;
};
