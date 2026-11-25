#pragma once
#include "Scene/GpuDrawList.h"

class ShadowMap
{
public:
    // One map covers the whole scene, so the resolution trades 8 MB of D16 and shadow fill rate for texel density.
    static constexpr uint32_t   Resolution = 2048;
    static constexpr hz::Format DepthFormat = hz::Format::D16_UNORM;

    explicit ShadowMap(hz::RenderContext& Context);
    Matrix4               Prepare(const hz::SceneView&, const Vector3& LightDirection);
    void                  Execute(hz::CommandList&, const hz::SceneView&, const hz::GpuDrawList&, const hz::GPUTexture& Depth);
    const hz::GPUSampler& GetSampler() const { return Sampler; }

private:
    hz::RenderContext&               Context;
    hz::GPUPipeline                  Pipeline;
    hz::GPUSampler                   Sampler;
    hz::GPUSampler                   SurfaceSampler;
    hz::FixedArray<hz::GPUBuffer, 3> ShadowFrames;
};
