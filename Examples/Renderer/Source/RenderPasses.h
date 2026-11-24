#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Graphics/FrameGraph.h"
#include "Graphics/RenderContext.h"
#include "Scene/GpuDrawList.h"
#include "Scene/GpuSkinning.h"
#include "FrustumCulling.h"
#include "ClusteredLighting.h"
#include "PostEffects.h"
#include "GTAO.h"
#include "TemporalAA.h"
#include "EnvironmentPass.h"
#include "ShadowMap.h"
#include "DeferredLighting.h"
#include "Transparency.h"
#include "RayTracedEffects.h"
#include "Skinning.h"

struct RenderPassesDesc
{
    hz::RenderContext* Context = nullptr;
    hz::RenderScene*   Scene = nullptr;
    hz::AssetManager*  AssetManager = nullptr;
    hz::RenderCameraID Camera;
};

class GBuffer
{
public:
    static constexpr uint32_t TextureCount = 4;

    GBuffer(hz::RenderContext& Context);
    void Execute(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                 const hz::GPUTexture* const (&Textures)[TextureCount], const hz::GPUTexture& Depth);

    hz::GPUPipeline Pipeline;
    hz::GPUSampler  Sampler;
};

// Frame graph handles of the renderer's resources; passes look up their concrete textures through these.
struct RenderTargets
{
    // The draw lists GpuDrawList owns; culling writes them and the drawing passes read them.
    hz::FrameGraphBuffer  DrawLists;
    hz::FrameGraphBuffer  LightMasks;
    // The streams the skinning pass writes; the passes that draw or trace geometry read them.
    hz::FrameGraphBuffer  SkinnedVertices;
    hz::FrameGraphTexture GBuffer[GBuffer::TextureCount];
    hz::FrameGraphTexture Depth;
    hz::FrameGraphTexture ShadowMap;
    hz::FrameGraphTexture Occlusion;
    hz::FrameGraphTexture OcclusionScratch;
    hz::FrameGraphTexture RTResult;
    hz::FrameGraphTexture RTScratch;
    hz::FrameGraphTexture RTReflection;
    hz::FrameGraphTexture RTReflectionHistory;
    hz::FrameGraphTexture RTHistory;
    hz::FrameGraphTexture RTPreviousDepth;
    hz::FrameGraphTexture SceneColor;
    hz::FrameGraphTexture OITHeads;
    hz::FrameGraphTexture TransparentColor;
    hz::FrameGraphTexture TemporalColor;
    hz::FrameGraphBuffer  OITNodes;
    hz::FrameGraphBuffer  OITCounters;
    hz::FrameGraphTexture Luminance[PostEffectsTargets::LuminanceLevels];
    hz::FrameGraphTexture BloomDown[PostEffectsTargets::BloomLevels];
    hz::FrameGraphTexture BloomUp[PostEffectsTargets::BloomLevels - 1];
    hz::FrameGraphTexture AdaptedLuminance;
    hz::FrameGraphTexture Backbuffer;
};

class RenderPasses
{
public:
    RenderPasses(const RenderPassesDesc& Desc);
    ~RenderPasses();

    bool Resize(uint32_t Width, uint32_t Height);

    void Update(float DeltaSeconds);
    void Execute();
    bool EnableClusteredLighting = true;
    bool EnableGTAO = true;
    bool EnableTransparency = true;
    bool EnableTemporalAA = true;

private:
    // Uploaded as the frame constant block, so the fields keep the shader's names.
    struct FrameData
    {
        Matrix4 ViewProjection;
        Matrix4 PreviousViewProjection;
        Matrix4 InverseViewProjection;
        Vector4 Eye;
    };

    struct FrameConstantBuffer
    {
        hz::GPUBuffer    Buffer;
        hz::SubmitHandle Submission;
    };

    void BuildGraph();

    hz::RenderContext& Context;
    hz::RenderScene&   Scene;
    hz::AssetManager&  Assets;
    hz::RenderCameraID CameraId;

    hz::UniquePtr<FrustumCulling>          Culling;
    hz::UniquePtr<SkinningPass>            SkinningCompute;
    hz::UniquePtr<GBuffer>                 GBufferPass;
    hz::UniquePtr<DeferredLighting>        Lighting;
    hz::UniquePtr<ClusteredLighting>       Clusters;
    hz::UniquePtr<PostEffects>             PostProcessing;
    hz::UniquePtr<GTAO>                    GroundTruthAO;
    hz::UniquePtr<EnvironmentPass>         Environment;
    hz::UniquePtr<ShadowMap>               Shadows;
    hz::UniquePtr<Transparency>            Transparency;
    hz::UniquePtr<RayTracedEffects>        RayTracing;
    hz::UniquePtr<TemporalAA>              Antialiasing;
    hz::GPUSampler                         LinearSampler;
    hz::GpuDrawList                        Draws;
    float                                  DeltaSeconds = 1.0f / 60.0f;
    hz::FixedArray<FrameConstantBuffer, 3> FrameConstants;
    uint32_t                               NextFrame = 0;
    hz::GpuScene                           GpuSceneData;
    hz::GpuSkinning                        Skinning;
    hz::FrameGraph*                        Graph = nullptr;
    RenderTargets                          Targets;
    hz::FrameGraphPass                     SkinningGraphPass;
    hz::FrameGraphPass                     LightMaskPass;
    hz::FrameGraphPass                     GTAOGraphPass;
    hz::FrameGraphPass                     RayTracingPass;
    hz::FrameGraphPass                     TransparencyPass;
    hz::FrameGraphPass                     TemporalAAPass;
    TemporalJitter                         PreviousJitter = TemporalJitter{};
    bool                                   TemporalAAWasEnabled = false;
};
