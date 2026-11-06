#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneFrame.h"
#include "GpuDrawList.h"
#include "PostEffects.h"
#include "ScreenSpaceAO.h"
#include "EnvironmentPass.h"
#include "ShadowMap.h"
#include "DeferredLighting.h"
#include "Transparency.h"
#include "RayTracedEffects.h"

struct RenderPassesDesc
{
    hz::RenderContext* pContext = nullptr;
    hz::RenderScene*   pScene = nullptr;
    hz::RenderCameraID camera;
};

class GBuffer
{
public:
    GBuffer(hz::RenderContext& context);
    void execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const hz::SceneFrameSnapshot& scene, const GpuDrawList& draws);
    void resize(uint32_t width, uint32_t height);

    hz::RenderContext&        context;
    static constexpr uint32_t gbufferCount = 4;
    hz::GPUPipeline           pipeline;
    hz::GPUSampler            sampler;
    hz::GPUTexture            gbuffer[gbufferCount];
    hz::GPUTexture            depth;
};

class RenderPasses
{
public:
    RenderPasses(const RenderPassesDesc& desc);
    ~RenderPasses() = default;

    bool resize(uint32_t width, uint32_t height);

    void update(float deltaSeconds);
    void execute();
    bool enableScreenSpaceAO = false;
    bool enableTransparency = true;

private:
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };

    hz::RenderContext& context;
    hz::RenderScene&   scene;
    hz::RenderCameraID camera;

    hz::unique_ptr<GBuffer>          gbuffer;
    hz::unique_ptr<DeferredLighting> lighting;
    hz::unique_ptr<PostEffects>      postprocessing;
    hz::unique_ptr<ScreenSpaceAO>    ssao;
    hz::unique_ptr<EnvironmentPass>  environment;
    hz::unique_ptr<ShadowMap>        shadows;
    hz::unique_ptr<Transparency>     transparency;
    hz::unique_ptr<RayTracedEffects> rayTracing;
    hz::GPUSampler                   linearSampler;
    GpuDrawList                      draws;
    float                            deltaSeconds = 1.0f / 60.0f;
    hz::FixedArray<hz::GPUBuffer, 3> frameBuffers;
    hz::SceneFramePool               frames;
};
