#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneFrame.h"

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
    void                      update();
    void                      execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const hz::SceneFrameSnapshot& scene);
    void                      load(uint32_t width, uint32_t height);
    void                      unload();
    hz::RenderContext&        context;
    static constexpr uint32_t gbufferCount = 4;
    hz::GPUPipeline           pipeline;
    hz::GPUSampler            sampler;
    hz::GPUTexture            gbuffer[gbufferCount];
    hz::GPUTexture            depth;

private:
    hz::Array<const hz::GPUBuffer*>  buffers;
    hz::Array<const hz::GPUTexture*> textures;
    hz::Array<uint32_t>              materialIds;
    uint64_t                         materialRevision = 0;
};

class Lighting
{
public:
    Lighting(hz::RenderContext& context);
    void           update();
    void           execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const GBuffer& gbuffer, const hz::SceneFrameSnapshot& scene);
    void           load(uint32_t width, uint32_t height);
    void           unload();
    hz::GPUTexture sceneColor;
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
    void update();
    void execute();

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
    hz::unique_ptr<Lighting>         lighting;
    hz::unique_ptr<PostProcessing>   postprocessing;
    hz::FixedArray<hz::GPUBuffer, 3> frameBuffers;
    hz::SceneFramePool               frames;
};
