#pragma once
#include "GpuDrawList.h"
#include "DeferredLighting.h"

class ShadowMap
{
public:
    // One map covers the whole scene, so the resolution trades 8 MB of D16 and shadow fill rate for texel density.
    static constexpr uint32_t   Resolution = 2048;
    static constexpr hz::Format DepthFormat = hz::Format::D16_UNORM;

    explicit ShadowMap(hz::RenderContext& Context);
    // Fits the light to the scene and uploads the lighting frame every frame; lighting reads it even when nothing draws shadows.
    void                  Prepare(const SceneView&);
    void                  Execute(hz::CommandList&, const SceneView&, const GpuDrawList&, const hz::GPUTexture& Depth);
    const hz::GPUSampler& GetSampler() const { return Sampler; }
    const hz::GPUBuffer&  GetLightingFrame(uint32_t Backing) const { return LightingFrames[Backing]; }
    float                 LuminousEfficacy = 100.0f; // lm/W; explicit Cookbook daylight-white calibration.
    LightingFrameData     Data;

private:
    hz::RenderContext&               Context;
    hz::GPUPipeline                  Pipeline;
    hz::GPUSampler                   Sampler;
    hz::GPUSampler                   SurfaceSampler;
    hz::FixedArray<hz::GPUBuffer, 3> ShadowFrames;
    hz::FixedArray<hz::GPUBuffer, 3> LightingFrames;
};
