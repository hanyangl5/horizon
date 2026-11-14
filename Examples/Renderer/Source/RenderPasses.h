#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Graphics/RenderContext.h"
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
    hz::AssetManager*  pAssetManager = nullptr;
    hz::RenderCameraID camera;
};

// Per-frame state shared by the registered passes. Earlier passes fill what later passes consume.
struct FrameContext
{
    hz::CommandList&      commands;
    GpuDrawList&          draws;
    const hz::GPUBuffer&  frame;
    const SceneView&      view;
    hz::GPUSampler&       linearSampler;
    float                 deltaSeconds = 0.0f;
    float                 environmentIntensity = 0.0f;
    bool                  rayTracedAO = false;
    const hz::GPUTexture* pColor = nullptr;
    const hz::GPUTexture* pBackbuffer = nullptr;
    DeferredGBuffer       gbuffer = {};
    LightingResources     resources = {};
};

// One entry of the frame pass list. pEnabled gates optional effects without editing the list.
struct RenderPass
{
    const char* pName = nullptr;
    void (*pExecute)(FrameContext&, void*) = nullptr;
    void*       pUserData = nullptr;
    const bool* pEnabled = nullptr;
};

class GBuffer
{
public:
    GBuffer(hz::RenderContext& context);
    void execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const SceneView& view, const GpuDrawList& draws);
    void resize(uint32_t width, uint32_t height);

    hz::RenderContext&        context;
    static constexpr uint32_t gbufferCount = 4;
    uint32_t                  width = 0;
    uint32_t                  height = 0;
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
    void addPass(const RenderPass& pass)
    {
        ASSERT(passCount < maxPasses);
        passes[passCount++] = pass;
    }
    bool enableScreenSpaceAO = false;
    bool enableTransparency = true;

    static constexpr uint32_t maxPasses = 12;

private:
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };

    struct FrameConstants
    {
        hz::GPUBuffer    buffer;
        hz::SubmitHandle submission;
    };

    hz::RenderContext& context;
    hz::RenderScene&   scene;
    hz::AssetManager&  assets;
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
    hz::FixedArray<FrameConstants, 3> frameConstants;
    uint32_t                          nextFrame = 0;
    hz::FixedArray<RenderPass, maxPasses> passes;
    uint32_t                              passCount = 0;
    hz::GpuScene                      gpuScene;
};
