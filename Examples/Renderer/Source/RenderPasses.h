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
    hz::RenderContext* Context = nullptr;
    hz::RenderScene*   Scene = nullptr;
    hz::AssetManager*  AssetManager = nullptr;
    hz::RenderCameraID Camera;
};

// Per-frame state shared by the registered passes. Earlier passes fill what later passes consume.
struct FrameContext
{
    hz::CommandList&      Commands;
    GpuDrawList&          Draws;
    const hz::GPUBuffer&  Frame;
    const SceneView&      View;
    hz::GPUSampler&       LinearSampler;
    float                 DeltaSeconds = 0.0f;
    float                 EnvironmentIntensity = 0.0f;
    bool                  RayTracedAO = false;
    const hz::GPUTexture* Color = nullptr;
    const hz::GPUTexture* Backbuffer = nullptr;
    DeferredGBuffer       GBuffer = {};
    LightingResources     Resources = {};
};

// One entry of the frame pass list. Enabled gates optional effects without editing the list.
struct RenderPass
{
    const char* Name = nullptr;
    void (*Execute)(FrameContext&, void*) = nullptr;
    void*       UserData = nullptr;
    const bool* Enabled = nullptr;
};

class GBuffer
{
public:
    GBuffer(hz::RenderContext& Context);
    void Execute(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const SceneView& View, const GpuDrawList& Draws);
    void Resize(uint32_t Width, uint32_t Height);

    hz::RenderContext&        Context;
    static constexpr uint32_t TextureCount = 4;
    uint32_t                  Width = 0;
    uint32_t                  Height = 0;
    hz::GPUPipeline           Pipeline;
    hz::GPUSampler            Sampler;
    hz::GPUTexture            Textures[TextureCount];
    hz::GPUTexture            Depth;
};

class RenderPasses
{
public:
    RenderPasses(const RenderPassesDesc& Desc);
    ~RenderPasses() = default;

    bool Resize(uint32_t Width, uint32_t Height);

    void Update(float DeltaSeconds);
    void Execute();
    void AddPass(const RenderPass& Pass)
    {
        ASSERT(PassCount < MaxPasses);
        Passes[PassCount++] = Pass;
    }
    bool EnableScreenSpaceAO = false;
    bool EnableTransparency = true;

    static constexpr uint32_t MaxPasses = 12;

private:
    // Uploaded as the frame constant block, so the fields keep the shader's names.
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };

    struct FrameConstantBuffer
    {
        hz::GPUBuffer    Buffer;
        hz::SubmitHandle Submission;
    };

    hz::RenderContext& Context;
    hz::RenderScene&   Scene;
    hz::AssetManager&  Assets;
    hz::RenderCameraID CameraId;

    hz::unique_ptr<GBuffer>          GBufferTargets;
    hz::unique_ptr<DeferredLighting> Lighting;
    hz::unique_ptr<PostEffects>      PostProcessing;
    hz::unique_ptr<ScreenSpaceAO>    SSAO;
    hz::unique_ptr<EnvironmentPass>  Environment;
    hz::unique_ptr<ShadowMap>        Shadows;
    hz::unique_ptr<Transparency>     Transparency;
    hz::unique_ptr<RayTracedEffects> RayTracing;
    hz::GPUSampler                   LinearSampler;
    GpuDrawList                      Draws;
    float                            DeltaSeconds = 1.0f / 60.0f;
    hz::FixedArray<FrameConstantBuffer, 3> FrameConstants;
    uint32_t                              NextFrame = 0;
    hz::FixedArray<RenderPass, 12>        Passes;
    uint32_t                              PassCount = 0;
    hz::GpuScene                          GpuSceneData;
};
