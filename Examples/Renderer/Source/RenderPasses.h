#pragma once

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
    const VertexLayout*       pVertexLayout = nullptr;
    TinyImageFormat           surfaceFormat = TinyImageFormat_UNDEFINED;
    float                     verticalFov = PI / 4.0f;
};

class GBuffer
{
public:
    GBuffer(hz::RenderContext& pContext, const VertexLayout& vertexLayout);
    void                      update();
    void                      execute(hz::CommandList& cmd, const hz::GPUBuffer& frame, const hz::GPUBuffer& draws,
                                      SceneManager& scenes, SceneAssetHandle scene, hz::Span<const SceneAssetInstance> instances);
    void                      load(uint32_t width, uint32_t height);
    void                      unload();
    hz::RenderContext&        pContext;
    static constexpr uint32_t GBufferCount = 4;
    hz::GPUShader             mGeometryShader;
    hz::GPUPipeline           mGeometryPipeline;
    hz::GPUSampler            mSampler;
    hz::GPUTexture mGBuffer[GBufferCount];
    hz::GPUTexture            mDepth;
};

class Lighting
{
public:
    Lighting(hz::RenderContext& pContext, TinyImageFormat format);
    void update();
    void execute(hz::CommandList& cmd, const hz::GPUTexture& renderTarget, const hz::GPUBuffer& frame, const GBuffer& gbuffer);
    void load(uint32_t width, uint32_t height);
    void unload();
    hz::RenderContext& pContext;
    hz::GPUShader mLightingShader;
    hz::GPUPipeline mLightingPipeline;
};

class RenderPasses
{
public:
    RenderPasses(const RenderPassesDesc& desc);
    ~RenderPasses();

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

    hz::RenderContext&  pContext;
    SceneManager*       pScenes = nullptr;
    SceneAssetHandle    mScene = {};
    SceneAssetInstance* pInstances = nullptr;
    uint32_t            mInstanceCount = 0;
    TinyImageFormat     mSurfaceFormat = TinyImageFormat_UNDEFINED;
    float               mVerticalFov = PI / 4.0f;

    hz::unique_ptr<GBuffer> gbuffer;
    hz::unique_ptr<Lighting> lighting;
    hz::GPUBuffer            mFrame;
    hz::GPUBuffer            mDraws;
    FrameData           mFrameData = {};
    Matrix4             mPreviousViewProjection;
    bool                mHasPreviousViewProjection = false;
};
