#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Graphics/RenderContext.h"
#include "Scene/ISceneManager.h"
#include "Scene/SceneGeometry.h"

class FreeCameraController;

struct RenderPassesDesc
{
    hz::RenderContext*        pContext = nullptr;
    SceneManager*             pScenes = nullptr;
    SceneAssetHandle          scene = {};
    const SceneAssetInstance* pInstances = nullptr;
    uint32_t                  instanceCount = 0;
    float                     verticalFov = PI / 4.0f;
};

class GBuffer
{
public:
    GBuffer(hz::RenderContext& context);
    void               update();
    void               execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const hz::GPUBuffer& draws, const hz::GPUBuffer& materials,
                               SceneManager& scenes, SceneAssetHandle scene, hz::Span<const SceneAssetInstance> instances);
    void               load(uint32_t width, uint32_t height);
    void               unload();
    hz::RenderContext& context;
    static constexpr uint32_t gbufferCount = 4;
    hz::GPUPipeline           pipeline;
    hz::GPUSampler            sampler;
    hz::GPUTexture            gbuffer[gbufferCount];
    hz::GPUTexture            depth;
};

class Lighting
{
public:
    Lighting(hz::RenderContext& context);
    void               update();
    void               execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const GBuffer& gbuffer);
    void               load(uint32_t width, uint32_t height);
    void               unload();
    hz::GPUTexture     sceneColor;
    hz::RenderContext& context;
    hz::GPUPipeline    pipeline;
};

class PostProcessing
{
public:
    PostProcessing(hz::RenderContext& context);
    void update();
    void execute(hz::CommandList& cmd, const hz::GPUTexture& renderTarget, const hz::GPUTexture& sceneColor, const hz::GPUTexture& depth);
    void load(uint32_t width, uint32_t height);
    void unload();
    hz::RenderContext& context;
    hz::GPUPipeline    pipeline;
};

class RenderPasses
{
public:
    RenderPasses(const RenderPassesDesc& desc);
    ~RenderPasses() = default;

    bool load(uint32_t width, uint32_t height);
    void unload();
    void update(const FreeCameraController& camera);
    void execute();

private:
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };

    bool                 initRenderResources();
    const SceneGeometry& getGeometry() const;

    hz::RenderContext&            pContext;
    SceneManager*                 pScenes = nullptr;
    SceneAssetHandle              scene = {};
    hz::Array<SceneAssetInstance> instances;
    float                         verticalFov = PI / 4.0f;

    hz::unique_ptr<GBuffer>        gbuffer;
    hz::unique_ptr<Lighting>       lighting;
    hz::unique_ptr<PostProcessing> postprocessing;
    hz::GPUBuffer                  frame;
    hz::GPUBuffer                  draws;
    hz::GPUBuffer                  materials;
    FrameData                      frameData = {};
    Matrix4                        previousViewProjection;
    bool                           hasPreviousViewProjection = false;
};
