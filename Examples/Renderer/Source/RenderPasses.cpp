#include "RenderPasses.h"
#include "Core/ICVar.h"
#include "RendererConstants.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Core/ILog.h"
#include "Profiler/IProfiler.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

constexpr hz::Format DepthFormat = hz::Format::D24_UNORM_S8_UINT;
constexpr hz::Format GBufferFormats[] = {
    hz::Format::B10G11R11_UFLOAT,
    hz::Format::R10G10B10A2_UNORM,
    hz::Format::R8G8B8A8_UNORM,
    hz::Format::R8G8B8A8_UNORM,
};
constexpr const char* GBufferNames[] = {
    "Renderer.GBuffer.Emissive",
    "Renderer.GBuffer.NormalMaterial",
    "Renderer.GBuffer.BaseColorMetallic",
    "Renderer.GBuffer.MotionMaterialId",
};

static_assert(offsetof(hz::RenderInstance, current) == 32 && offsetof(hz::RenderInstance, previous) == 96);

struct GeometryBindingConstants
{
    uint32_t FrameIndex;
    uint32_t InstancesIndex;
    uint32_t MaterialsIndex;
    uint32_t TexturesIndex;
    uint32_t SamplerIndex;
};
static_assert(sizeof(GeometryBindingConstants) == 5 * sizeof(uint32_t));

static hz::CVar GClusteredLighting("r.ClusteredLighting", true, "0 uses the full light loop, 1 enables light culling.");
static hz::CVar GTemporalAA("r.TAA", true, "Temporal antialiasing.");
static hz::CVar GTransparency("r.Transparency", true, "Blended transparent surfaces.");
static hz::CVar GGTAO("r.GTAO", true, "Screen-space ambient occlusion when ray-traced AO is off.");

GBuffer::GBuffer(hz::RenderContext& Context)
{
    Pipeline = Context.CreateGraphicsPipeline({
    .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect", .Name = "Renderer.GeometryVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .Name = "Renderer.GeometryPS" },
        },
        .pFileName = "Geometry.slang",
    },
    .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
    .colorTargets = {
        { .Format = GBufferFormats[0] },
        { .Format = GBufferFormats[1] },
        { .Format = GBufferFormats[2] },
        { .Format = GBufferFormats[3] },
    },
    .depthStencilFormat = DepthFormat,
    .indirectDrawId = true,
    .Name = "Renderer.GeometryPipeline",
});

    Sampler = Context.CreateSampler();
    ASSERT(Pipeline.IsValid() && Sampler.IsValid());
}

void GBuffer::Execute(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                      const hz::GPUTexture* const (&Textures)[TextureCount], const hz::GPUTexture& Depth)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GBuffer::execute");
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
    const ClearValue           Black = {};
    const uint32_t             Width = Depth.GetWidth();
    const uint32_t             Height = Depth.GetHeight();
    Cmd.BeginGpuTimestamp("Geometry");
    Cmd.BeginRendering({
        .colorAttachments =  {
            { .pTexture = Textures[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .ClearValue = Black },
            { .pTexture = Textures[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .ClearValue = Black },
            { .pTexture = Textures[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .ClearValue = Black },
            { .pTexture = Textures[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .ClearValue = Black },
        } ,
        .depthAttachment = { .pTexture = &Depth,
                             .loadAction = LOAD_ACTION_CLEAR,
                             .storeAction = STORE_ACTION_STORE,
                             .ClearValue = { .Depth = 1.0f } },
    }, Draws.GetDependencies(Frame, View, &Draws.GetCommands(View.FrameIndex, Draws.GetOpaqueList())));
    Cmd.SetViewport(0, 0, (float)Width, (float)Height);
    Cmd.SetScissor(0, 0, Width, Height);
    Cmd.SetPipeline(Pipeline);
    const GeometryBindingConstants Binding = {
        .FrameIndex = Frame.GetSrvIndex(),
        .InstancesIndex = Gpu.instances.IsValid() ? Gpu.instances.GetSrvIndex() : UINT32_MAX,
        .MaterialsIndex = Gpu.materials.IsValid() ? Gpu.materials.GetSrvIndex() : UINT32_MAX,
        .TexturesIndex = Gpu.textures.IsValid() ? Gpu.textures.GetSrvIndex() : UINT32_MAX,
        .SamplerIndex = Sampler.GetIndex(),
    };
    Cmd.SetRootConstants(Binding, GeometryBindingSlot);

    Cmd.SetRootConstant({ Draws.GetDrawData(View.FrameIndex).GetSrvIndex() }, GeometryDrawSlot);
    if (Draws.GetCount(View.FrameIndex))
    {
        Cmd.DrawIndirect(Draws.GetCommands(View.FrameIndex, Draws.GetOpaqueList()), 0, Draws.GetCount(View.FrameIndex));
    }
    Cmd.EndRendering();
    Cmd.EndGpuTimestamp();
}

// Per-frame inputs the frame graph passes read; graph-owned textures are looked up through Targets.
struct FrameInputs
{
    hz::GpuDrawList&     Draws;
    const hz::GPUBuffer& Frame;
    const hz::SceneView& View;
    hz::GPUSampler&      LinearSampler;
    const RenderTargets& Targets;
    LightingResources    Lighting = {};
    float                DeltaSeconds = 0.0f;
    float                EnvironmentIntensity = 0.0f;
    Vector4              EnvironmentTint = Vector4(1.0f);
    bool                 RayTracedShadows = false;
    bool                 RayTracedReflection = false;
    bool                 TemporalAAEnabled = false;
    TemporalJitter       Jitter;
    TemporalJitter       PreviousJitter;
};

template<typename T>
struct PassData
{
    T*           Module;
    FrameInputs* PendingFrame = nullptr;
};

static const FrameInputs& Inputs(const hz::FrameGraphContext& Context) { return *(const FrameInputs*)Context.FrameData; }

static DeferredGBuffer GetGBuffer(const hz::FrameGraphContext& Context)
{
    const RenderTargets& Targets = Inputs(Context).Targets;
    return { .Emissive = hz::GetFrameGraphTexture(Context, Targets.GBuffer[0]),
             .NormalMaterial = hz::GetFrameGraphTexture(Context, Targets.GBuffer[1]),
             .BaseColorMetallic = hz::GetFrameGraphTexture(Context, Targets.GBuffer[2]),
             .Depth = hz::GetFrameGraphTexture(Context, Targets.Depth) };
}

static hz::FrameGraphTexture ViewportTexture(hz::FrameGraph* Graph, hz::Format Format, const char* Name, bool RenderTarget,
                                             uint32_t SizeShift = 0, bool History = false)
{
    const DescriptorType Storage = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    return hz::AddFrameGraphTexture(
        Graph, { .Texture = { .Format = Format,
                              .StartState = RenderTarget ? RESOURCE_STATE_RENDER_TARGET : RESOURCE_STATE_UNORDERED_ACCESS,
                              .Descriptors = RenderTarget ? DESCRIPTOR_TYPE_TEXTURE : Storage,
                              .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                              .GetRenderTarget = RenderTarget,
                              .Name = Name },
                 .SizeShift = SizeShift,
                 .History = History });
}

void RenderPasses::BuildGraph()
{
    using Use = hz::FrameGraphTextureUse;
    constexpr hz::FrameGraphAccess Read = hz::FrameGraphAccessRead;
    constexpr hz::FrameGraphAccess Optional = hz::FrameGraphAccessReadOptional;
    constexpr hz::FrameGraphAccess History = hz::FrameGraphAccessReadHistory;
    constexpr hz::FrameGraphAccess Write = hz::FrameGraphAccessWrite;
    constexpr hz::FrameGraphAccess Modify = hz::FrameGraphAccessModify;
    hz::FrameGraph*                G = Graph;
    RenderTargets&                 T = Targets;
    for (uint32_t Index = 0; Index < GBuffer::TextureCount; ++Index)
    {
        T.GBuffer[Index] = ViewportTexture(G, GBufferFormats[Index], GBufferNames[Index], true);
    }
    T.Depth = hz::AddFrameGraphTexture(G, { .Texture = { .Format = DepthFormat,
                                                         .StartState = RESOURCE_STATE_DEPTH_WRITE,
                                                         .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                         .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                                         .GetRenderTarget = true,
                                                         .Name = "Renderer.Depth" } });
    T.ShadowMap = hz::AddFrameGraphTexture(G, { .Texture = { .Width = ShadowMap::Resolution,
                                                             .Height = ShadowMap::Resolution,
                                                             .Format = ShadowMap::DepthFormat,
                                                             .StartState = RESOURCE_STATE_DEPTH_WRITE,
                                                             .Descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                             .GetRenderTarget = true,
                                                             .Name = "Renderer.ShadowMap" } });
    T.Occlusion = ViewportTexture(G, GTAO::TargetFormat, "Renderer.GTAO", false);
    T.OcclusionScratch = ViewportTexture(G, GTAO::TargetFormat, "Renderer.GTAO.Half", false, 1);
    T.RTResult = ViewportTexture(G, RayTracedEffects::ResultFormat, "Renderer.RT.ShadowAO", false, RayTracedEffects::ResolutionShift);
    T.RTScratch =
        ViewportTexture(G, RayTracedEffects::ScratchFormat, "Renderer.RT.DenoiseScratch", false, RayTracedEffects::ResolutionShift);
    T.RTReflection =
        ViewportTexture(G, RayTracedEffects::ReflectionFormat, "Renderer.RT.Reflection", false, RayTracedEffects::ResolutionShift);
    T.RTReflectionHistory = ViewportTexture(G, RayTracedEffects::ReflectionFormat, "Renderer.RT.ReflectionHistory", false,
                                            RayTracedEffects::ResolutionShift, true);
    T.RTHistory =
        ViewportTexture(G, RayTracedEffects::HistoryFormat, "Renderer.RT.History", false, RayTracedEffects::ResolutionShift, true);
    T.RTPreviousDepth =
        ViewportTexture(G, RayTracedEffects::DepthFormat, "Renderer.RT.PreviousDepth", false, RayTracedEffects::ResolutionShift, true);
    T.SceneColor = ViewportTexture(G, DeferredLighting::ColorFormat, "Renderer.DeferredLighting.Color", true);
    T.OITHeads = ViewportTexture(G, Transparency::HeadsFormat, "Renderer.OIT.Heads", false);
    T.TransparentColor = ViewportTexture(G, Transparency::ColorFormat, "Renderer.OIT.Resolved", false);
    T.TemporalColor = ViewportTexture(G, DeferredLighting::ColorFormat, "Renderer.TAA.History", false, 0, true);
    const DescriptorType RawStorage = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW);
    // One fragment per pixel on average, as before.
    T.OITNodes = hz::AddFrameGraphBuffer(G, { .Buffer = { .StructStride = 4,
                                                          .Name = "Renderer.OIT.Nodes",
                                                          .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                          .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                          .Descriptors = RawStorage },
                                              .BytesPerPixel = Transparency::NodeBytes });
    T.OITCounters = hz::AddFrameGraphBuffer(G, { .Buffer = { .Size = 16,
                                                             .ElementCount = 4,
                                                             .Name = "Renderer.OIT.Counters",
                                                             .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                             .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                             .Descriptors = RawStorage } });
    const DescriptorType Storage = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    for (uint32_t Level = 0; Level < PostEffectsTargets::LuminanceLevels; ++Level)
    {
        const uint32_t Size = PostEffectsTargets::MeteringSize >> Level;
        T.Luminance[Level] = hz::AddFrameGraphTexture(G, { .Texture = { .Width = Size ? Size : 1,
                                                                        .Height = Size ? Size : 1,
                                                                        .Format = PostEffects::LuminanceFormat,
                                                                        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                                        .Descriptors = Storage,
                                                                        .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                                                        .Name = "Renderer.HDR.LogLuminance" } });
    }
    // Bloom follows the render size so the glare keeps its screen-space radius; level 0 is half the viewport.
    for (uint32_t Level = 0; Level < PostEffectsTargets::BloomLevels; ++Level)
    {
        T.BloomDown[Level] = ViewportTexture(G, PostEffects::BloomFormat, "Renderer.HDR.BloomDown", false, Level + 1);
        if (Level + 1 < PostEffectsTargets::BloomLevels)
        {
            T.BloomUp[Level] = ViewportTexture(G, PostEffects::BloomFormat, "Renderer.HDR.BloomUp", false, Level + 1);
        }
    }
    T.AdaptedLuminance = hz::AddFrameGraphTexture(G, { .Texture = { .Width = 1,
                                                                    .Height = 1,
                                                                    .Format = PostEffects::AdaptedFormat,
                                                                    .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                                    .Descriptors = Storage,
                                                                    .Flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                                                    .Name = "Renderer.HDR.AdaptedLuminance" },
                                                       .History = true });
    T.Backbuffer = hz::ImportFrameGraphTexture(G, { .Name = "Backbuffer", .Output = true });
    // GpuDrawList recreates its list buffers as the scene grows, so they stay outside the graph's resources; the import
    // only orders culling before the passes that draw from the lists.
    T.DrawLists = hz::ImportFrameGraphBuffer(G, { .Name = "DrawLists" });

    const hz::FrameGraphBufferUse WriteLists[] = { { T.DrawLists, hz::FrameGraphAccessWrite } };
    T.SkinnedVertices = hz::ImportFrameGraphBuffer(G, { .Name = "SkinnedVertices" });
    const hz::FrameGraphBufferUse WriteSkinned[] = { { T.SkinnedVertices, hz::FrameGraphAccessWrite } };
    const hz::FrameGraphBufferUse ReadSkinned[] = { { T.SkinnedVertices, hz::FrameGraphAccessRead } };
    const hz::FrameGraphBufferUse ReadLists[] = { { T.DrawLists, hz::FrameGraphAccessRead },
                                                  { T.SkinnedVertices, hz::FrameGraphAccessRead } };
    SkinningGraphPass = hz::AddFrameGraphPass(G,
                                              {
                                                  .Name = "Skinning",
                                                  .Execute =
                                                      [](const hz::FrameGraphContext& Pass, void* UserData)
                                                  {
                                                      const FrameInputs& Frame = Inputs(Pass);
                                                      static_cast<SkinningPass*>(UserData)->Execute(Pass.Commands, *Frame.View.Skinning,
                                                                                                    Frame.View.FrameIndex);
                                                  },
                                                  .UserData = SkinningCompute.Get(),
                                                  .Buffers = WriteSkinned,
                                              });
    hz::AddFrameGraphPass(G,
                          {
                              .Name = "Culling",
                              .Execute =
                                  [](const hz::FrameGraphContext& Pass, void* UserData)
                              {
                                  const FrameInputs& Frame = Inputs(Pass);
                                  static_cast<FrustumCulling*>(UserData)->Execute(Pass.Commands, Frame.Frame, Frame.View, Frame.Draws);
                              },
                              .UserData = Culling.Get(),
                              .Buffers = WriteLists,
                          });
    T.LightMasks = hz::ImportFrameGraphBuffer(G, { .Name = "LightTileMasks" });
    const hz::FrameGraphBufferUse WriteMasks[] = { { T.LightMasks, Write } };
    const hz::FrameGraphBufferUse ReadMasks[] = { { T.LightMasks, Optional } };
    LightMaskPass =
        hz::AddFrameGraphPass(G,
                              {
                                  .Name = "BuildLightTileMasks",
                                  .Execute =
                                      [](const hz::FrameGraphContext& Pass, void* UserData)
                                  {
                                      const FrameInputs& Frame = Inputs(Pass);
                                      if (Frame.Lighting.TileMasks)
                                      {
                                          static_cast<ClusteredLighting*>(UserData)->BuildMasks(Pass.Commands, Frame.View.FrameIndex);
                                      }
                                  },
                                  .UserData = Clusters.Get(),
                                  .Buffers = WriteMasks,
                              });
    const Use ShadowUses[] = { { T.ShadowMap, Write } };
    hz::AddFrameGraphPass(G,
                          {
                              .Name = "Shadows",
                              .Execute =
                                  [](const hz::FrameGraphContext& Pass, void* UserData)
                              {
                                  const FrameInputs& Frame = Inputs(Pass);
                                  static_cast<ShadowMap*>(UserData)->Execute(Pass.Commands, Frame.View, Frame.Draws,
                                                                             *hz::GetFrameGraphTexture(Pass, Frame.Targets.ShadowMap));
                              },
                              .UserData = Shadows.Get(),
                              .Textures = ShadowUses,
                              .Buffers = ReadLists,
                          });
    const Use GBufferUses[] = { { T.GBuffer[0], Write },
                                { T.GBuffer[1], Write },
                                { T.GBuffer[2], Write },
                                { T.GBuffer[3], Write },
                                { T.Depth, Write } };
    hz::AddFrameGraphPass(G,
                          {
                              .Name = "GBuffer",
                              .Execute =
                                  [](const hz::FrameGraphContext& Pass, void* UserData)
                              {
                                  const FrameInputs&    Frame = Inputs(Pass);
                                  const hz::GPUTexture* Textures[GBuffer::TextureCount];
                                  for (uint32_t Index = 0; Index < GBuffer::TextureCount; ++Index)
                                  {
                                      Textures[Index] = hz::GetFrameGraphTexture(Pass, Frame.Targets.GBuffer[Index]);
                                  }
                                  static_cast<GBuffer*>(UserData)->Execute(Pass.Commands, Frame.Frame, Frame.View, Frame.Draws, Textures,
                                                                           *hz::GetFrameGraphTexture(Pass, Frame.Targets.Depth));
                              },
                              .UserData = GBufferPass.Get(),
                              .Textures = GBufferUses,
                              .Buffers = ReadLists,
                          });
    const Use GTAOUses[] = { { T.Depth, Read }, { T.GBuffer[1], Read }, { T.Occlusion, Write }, { T.OcclusionScratch, Write } };
    GTAOGraphPass = hz::AddFrameGraphPass(G,
                                          {
                                              .Name = "GTAO",
                                              .Execute =
                                                  [](const hz::FrameGraphContext& Pass, void* UserData)
                                              {
                                                  const FrameInputs& Frame = Inputs(Pass);
                                                  static_cast<GTAO*>(UserData)->Execute(
                                                      Pass.Commands, Frame.Frame, *hz::GetFrameGraphTexture(Pass, Frame.Targets.Depth),
                                                      *hz::GetFrameGraphTexture(Pass, Frame.Targets.GBuffer[1]),
                                                      *hz::GetFrameGraphTexture(Pass, Frame.Targets.Occlusion),
                                                      *hz::GetFrameGraphTexture(Pass, Frame.Targets.OcclusionScratch));
                                              },
                                              .UserData = GroundTruthAO.Get(),
                                              .Textures = GTAOUses,
                                          });
    const Use RTUses[] = { { T.GBuffer[1], Read },
                           { T.GBuffer[3], Read },
                           { T.Depth, Read },
                           { T.RTResult, Write },
                           { T.RTScratch, Write },
                           { T.RTReflection, Write },
                           { T.RTHistory, History },
                           { T.RTHistory, Write },
                           { T.RTPreviousDepth, History },
                           { T.RTReflectionHistory, History },
                           { T.RTReflectionHistory, Write },
                           { T.RTPreviousDepth, Write } };
    RayTracingPass = hz::AddFrameGraphPass(
        G,
        {
            .Name = "Ray Tracing",
            .Execute =
                [](const hz::FrameGraphContext& Pass, void* UserData)
            {
                const FrameInputs&            Frame = Inputs(Pass);
                const RenderTargets&          Targets = Frame.Targets;
                LightingResources             Lighting = Frame.Lighting;
                const RayTracedEffectsTargets RTTargets = {
                    .Result = hz::GetFrameGraphTexture(Pass, Targets.RTResult),
                    .Scratch = hz::GetFrameGraphTexture(Pass, Targets.RTScratch),
                    .Reflection = hz::GetFrameGraphTexture(Pass, Targets.RTReflection),
                    .History = hz::GetFrameGraphTexture(Pass, Targets.RTHistory),
                    .PreviousHistory = hz::GetFrameGraphHistory(Pass, Targets.RTHistory),
                    .PreviousDepth = hz::GetFrameGraphTexture(Pass, Targets.RTPreviousDepth),
                    .PreviousFrameDepth = hz::GetFrameGraphHistory(Pass, Targets.RTPreviousDepth),
                    .ReflectionHistory = hz::GetFrameGraphTexture(Pass, Targets.RTReflectionHistory),
                    .PreviousReflectionHistory = hz::GetFrameGraphHistory(Pass, Targets.RTReflectionHistory),
                    .HistoryValid = hz::IsFrameGraphHistoryValid(Pass, Targets.RTHistory) &&
                                    hz::IsFrameGraphHistoryValid(Pass, Targets.RTPreviousDepth),
                    .ReflectionHistoryValid = hz::IsFrameGraphHistoryValid(Pass, Targets.RTReflectionHistory),
                };
                static_cast<RayTracedEffects*>(UserData)->Execute(Pass.Commands, Frame.Frame,
                                                                  { .NormalMaterial = hz::GetFrameGraphTexture(Pass, Targets.GBuffer[1]),
                                                                    .Depth = hz::GetFrameGraphTexture(Pass, Targets.Depth),
                                                                    .Motion = hz::GetFrameGraphTexture(Pass, Targets.GBuffer[3]) },
                                                                  Frame.View, Frame.Draws, Lighting, RTTargets);
            },
            .UserData = RayTracing.Get(),
            .Textures = RTUses,
            .Buffers = ReadSkinned,
        });
    const Use LightingUses[] = { { T.GBuffer[0], Read },   { T.GBuffer[1], Read },       { T.GBuffer[2], Read },
                                 { T.Depth, Read },        { T.ShadowMap, Read },        { T.Occlusion, Optional },
                                 { T.RTResult, Optional }, { T.RTReflection, Optional }, { T.SceneColor, Write } };
    hz::AddFrameGraphPass(G,
                          {
                              .Name = "Lighting",
                              .Execute =
                                  [](const hz::FrameGraphContext& Pass, void* UserData)
                              {
                                  const FrameInputs&   Frame = Inputs(Pass);
                                  const RenderTargets& Targets = Frame.Targets;
                                  LightingResources    Lighting = Frame.Lighting;
                                  Lighting.ShadowMap = hz::GetFrameGraphTexture(Pass, Targets.ShadowMap);
                                  Lighting.GTAO = hz::GetFrameGraphTexture(Pass, Targets.Occlusion);
                                  // The pass writes both images whenever it runs; hand over only the effects that are enabled, as the
                                  // shader expects.
                                  Lighting.RTShadowAO = Frame.RayTracedShadows ? hz::GetFrameGraphTexture(Pass, Targets.RTResult) : nullptr;
                                  Lighting.RTReflection =
                                      Frame.RayTracedReflection ? hz::GetFrameGraphTexture(Pass, Targets.RTReflection) : nullptr;
                                  static_cast<DeferredLighting*>(UserData)->Execute(Pass.Commands, Frame.Frame, GetGBuffer(Pass), Lighting,
                                                                                    *hz::GetFrameGraphTexture(Pass, Targets.SceneColor));
                              },
                              .UserData = Lighting.Get(),
                              .Textures = LightingUses,
                              .Buffers = ReadMasks,
                          });
    const Use EnvironmentUses[] = { { T.SceneColor, Modify }, { T.Depth, Read } };
    hz::AddFrameGraphPass(G,
                          {
                              .Name = "Environment",
                              .Execute =
                                  [](const hz::FrameGraphContext& Pass, void* UserData)
                              {
                                  const FrameInputs& Frame = Inputs(Pass);
                                  static_cast<EnvironmentPass*>(UserData)->Execute(
                                      Pass.Commands, Frame.Frame, *hz::GetFrameGraphTexture(Pass, Frame.Targets.SceneColor),
                                      *hz::GetFrameGraphTexture(Pass, Frame.Targets.Depth), Frame.EnvironmentIntensity,
                                      Frame.EnvironmentTint);
                              },
                              .UserData = Environment.Get(),
                              .Textures = EnvironmentUses,
                          });
    const Use                     TransparencyUses[] = { { T.Depth, Read },
                                                         { T.ShadowMap, Read },
                                                         { T.SceneColor, Read },
                                                         { T.OITHeads, Write },
                                                         { T.TransparentColor, Write } };
    const hz::FrameGraphBufferUse TransparencyBuffers[] = { { T.OITNodes, Write }, { T.OITCounters, Write } };
    const hz::FrameGraphBufferUse TransparencyAllBuffers[] = { TransparencyBuffers[0], TransparencyBuffers[1], ReadLists[0], ReadLists[1],
                                                               ReadMasks[0] };
    TransparencyPass = hz::AddFrameGraphPass(
        G,
        {
            .Name = "Transparency",
            .Execute =
                [](const hz::FrameGraphContext& Pass, void* UserData)
            {
                const FrameInputs&   Frame = Inputs(Pass);
                const RenderTargets& Targets = Frame.Targets;
                LightingResources    Lighting = Frame.Lighting;
                Lighting.ShadowMap = hz::GetFrameGraphTexture(Pass, Targets.ShadowMap);
                const TransparencyTargets OITTargets = { .Heads = hz::GetFrameGraphTexture(Pass, Targets.OITHeads),
                                                         .Color = hz::GetFrameGraphTexture(Pass, Targets.TransparentColor),
                                                         .Nodes = hz::GetFrameGraphBuffer(Pass, Targets.OITNodes),
                                                         .Counters = hz::GetFrameGraphBuffer(Pass, Targets.OITCounters) };
                static_cast<::Transparency*>(UserData)->Execute(Pass.Commands, Frame.Frame, *hz::GetFrameGraphTexture(Pass, Targets.Depth),
                                                                Frame.View, Frame.Draws, Lighting,
                                                                *hz::GetFrameGraphTexture(Pass, Targets.SceneColor), OITTargets);
            },
            .UserData = Transparency.Get(),
            .Textures = TransparencyUses,
            .Buffers = TransparencyAllBuffers,
        });
    const Use TemporalUses[] = { { T.SceneColor, Read }, { T.TransparentColor, Optional }, { T.Depth, Read },
                                 { T.GBuffer[3], Read }, { T.TemporalColor, History },     { T.TemporalColor, Write } };
    TemporalAAPass = hz::AddFrameGraphPass(
        G,
        {
            .Name = "Temporal AA",
            .Execute =
                [](const hz::FrameGraphContext& Pass, void* UserData)
            {
                const FrameInputs&     Frame = Inputs(Pass);
                const RenderTargets&   Targets = Frame.Targets;
                const hz::GPUTexture*  Opaque = hz::GetFrameGraphTexture(Pass, Targets.SceneColor);
                const hz::GPUTexture*  Transparent = hz::GetFrameGraphTexture(Pass, Targets.TransparentColor);
                const hz::GPUTexture*  History = hz::IsFrameGraphHistoryValid(Pass, Targets.TemporalColor)
                                                     ? hz::GetFrameGraphHistory(Pass, Targets.TemporalColor)
                                                     : nullptr;
                const hz::GPUTexture&  Output = *hz::GetFrameGraphTexture(Pass, Targets.TemporalColor);
                const TemporalAAInputs ResolveInputs = {
                    .Color = Transparent ? Transparent : Opaque,
                    .Opaque = Transparent ? Opaque : nullptr,
                    .Depth = hz::GetFrameGraphTexture(Pass, Targets.Depth),
                    .Motion = hz::GetFrameGraphTexture(Pass, Targets.GBuffer[3]),
                    .History = History,
                };
                static_cast<TemporalAA*>(UserData)->Execute(Pass.Commands, Frame.Frame, Frame.LinearSampler, ResolveInputs, Output,
                                                            Frame.Jitter, Frame.PreviousJitter);
            },
            .UserData = Antialiasing.Get(),
            .Textures = TemporalUses,
        });
    Use      PostUses[4 + PostEffectsTargets::LuminanceLevels + 2 * PostEffectsTargets::BloomLevels - 1 + 2];
    uint32_t PostUseCount = 0;
    PostUses[PostUseCount++] = { T.SceneColor, Read };
    PostUses[PostUseCount++] = { T.TransparentColor, Optional };
    PostUses[PostUseCount++] = { T.TemporalColor, Optional };
    PostUses[PostUseCount++] = { T.Backbuffer, Write };
    for (hz::FrameGraphTexture Texture : T.Luminance)
    {
        PostUses[PostUseCount++] = { Texture, Write };
    }
    for (hz::FrameGraphTexture Texture : T.BloomDown)
    {
        PostUses[PostUseCount++] = { Texture, Write };
    }
    for (hz::FrameGraphTexture Texture : T.BloomUp)
    {
        PostUses[PostUseCount++] = { Texture, Write };
    }
    PostUses[PostUseCount++] = { T.AdaptedLuminance, History };
    PostUses[PostUseCount++] = { T.AdaptedLuminance, Write };
    ASSERT(PostUseCount == TF_ARRAY_COUNT(PostUses));
    hz::AddFrameGraphPass(
        G,
        {
            .Name = "Post",
            .Execute =
                [](const hz::FrameGraphContext& Pass, void* UserData)
            {
                const FrameInputs&   Frame = Inputs(Pass);
                const RenderTargets& Targets = Frame.Targets;
                PostEffectsTargets   PostTargets = { .PreviousAdapted = hz::GetFrameGraphHistory(Pass, Targets.AdaptedLuminance),
                                                     .Adapted = hz::GetFrameGraphTexture(Pass, Targets.AdaptedLuminance),
                                                     .AdaptedValid = hz::IsFrameGraphHistoryValid(Pass, Targets.AdaptedLuminance) };
                for (uint32_t Level = 0; Level < PostEffectsTargets::LuminanceLevels; ++Level)
                {
                    PostTargets.Luminance[Level] = hz::GetFrameGraphTexture(Pass, Targets.Luminance[Level]);
                }
                for (uint32_t Level = 0; Level < PostEffectsTargets::BloomLevels; ++Level)
                {
                    PostTargets.BloomDown[Level] = hz::GetFrameGraphTexture(Pass, Targets.BloomDown[Level]);
                }
                for (uint32_t Level = 0; Level + 1 < PostEffectsTargets::BloomLevels; ++Level)
                {
                    PostTargets.BloomUp[Level] = hz::GetFrameGraphTexture(Pass, Targets.BloomUp[Level]);
                }
                const hz::GPUTexture* Temporal = Frame.TemporalAAEnabled ? hz::GetFrameGraphTexture(Pass, Targets.TemporalColor) : nullptr;
                const hz::GPUTexture* Transparent = hz::GetFrameGraphTexture(Pass, Targets.TransparentColor);
                const hz::GPUTexture& SceneColor = Temporal      ? *Temporal
                                                   : Transparent ? *Transparent
                                                                 : *hz::GetFrameGraphTexture(Pass, Targets.SceneColor);
                static_cast<PostEffects*>(UserData)->Execute(Pass.Commands, SceneColor, *hz::GetFrameGraphTexture(Pass, Targets.Backbuffer),
                                                             Frame.DeltaSeconds, PostTargets);
            },
            .UserData = PostProcessing.Get(),
            .Textures = PostUses,
        });
}

RenderPasses::RenderPasses(const RenderPassesDesc& Desc):
    Context(*Desc.Context), Scene(*Desc.Scene), Assets(*Desc.AssetManager), CameraId(Desc.Camera), Draws(Context),
    GpuSceneData(Scene, Assets, Context, { .frameCount = FrameConstants.Size() }),
    Skinning(Scene, Assets, Context, { .FrameCount = FrameConstants.Size() })
{
    ASSERT(Context.IsValid() && CameraId.IsValid());
    Culling = hz::MakeUnique<FrustumCulling>(Context);
    SkinningCompute = hz::MakeUnique<SkinningPass>(Context);
    GBufferPass = hz::MakeUnique<GBuffer>(Context);
    Lighting = hz::MakeUnique<DeferredLighting>(Context);
    Clusters = hz::MakeUnique<ClusteredLighting>(Context);
    GroundTruthAO = hz::MakeUnique<GTAO>(Context);
    Environment = hz::MakeUnique<EnvironmentPass>(Context, DeferredLighting::ColorFormat);
    Shadows = hz::MakeUnique<ShadowMap>(Context);
    PostProcessing = hz::MakeUnique<PostEffects>(Context);
    Transparency = hz::MakeUnique<::Transparency>(Context);
    RayTracing = hz::MakeUnique<RayTracedEffects>(Context);
    Antialiasing = hz::MakeUnique<TemporalAA>(Context);
    Graph = hz::InitFrameGraph(&Context);
    BuildGraph();

    LinearSampler = Context.CreateSampler({ .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE });
    for (FrameConstantBuffer& ConstantBuffer : FrameConstants)
    {
        hz::GPUBuffer& Buffer = ConstantBuffer.Buffer;
        Buffer = Context.CreateBuffer({ .Size = sizeof(FrameData),
                                        .ElementCount = 1,
                                        .StructStride = sizeof(FrameData),
                                        .Name = "Renderer.Frame",
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .Descriptors = DESCRIPTOR_TYPE_BUFFER,
                                        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(Buffer.IsValid());
    }
}

RenderPasses::~RenderPasses() { hz::ExitFrameGraph(Graph); }

bool RenderPasses::Resize(uint32_t Width, uint32_t Height)
{
    if (!Width || !Height)
    {
        return true;
    }

    if (!Environment->IsValid() && !Environment->Load({ .Directory = DirectoryShaderSources,
                                                        .Prefiltered = "../../../Assets/Cookbook/immenstadter_horn_2k_prefilter.ktx",
                                                        .Irradiance = "../../../Assets/Cookbook/immenstadter_horn_2k_irradiance.ktx",
                                                        .BrdfLut = "../../../Assets/Cookbook/brdfLUT.ktx" }))
    {
        return false;
    }
    if (!RayTracing->Load())
    {
        return false;
    }
    PostProcessing->Resize();
    if (!hz::ResizeFrameGraph(Graph, Width, Height))
    {
        LOGF(Error, "Cannot compile the frame graph for %ux%u", Width, Height);
        return false;
    }

    Scene.MarkDirty();
    return true;
}

void RenderPasses::Update(float FrameDeltaSeconds) { DeltaSeconds = FrameDeltaSeconds; }

static Matrix4 CameraViewProjection(const Matrix4& Pose, const hz::Camera& Lens, float AspectInverse, TemporalJitter Jitter)
{
    const float HorizontalFov = 2.0f * atanf(tanf(Lens.verticalFov * 0.5f) / AspectInverse);
    Matrix4       Projection = Matrix4::perspectiveRH(HorizontalFov, AspectInverse, Lens.nearPlane, Lens.farPlane);
    const Vector4 ClipW = Projection.getRow(3);
    Projection.setRow(0, Projection.getRow(0) + ClipW * (2.0f * Jitter.X));
    Projection.setRow(1, Projection.getRow(1) - ClipW * (2.0f * Jitter.Y));
    return Projection * inverse(Pose);
}

void RenderPasses::Execute()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::execute");
    if (Context.IsSuspended())
    {
        return;
    }

    ASSERT(PostProcessing);
    // Wait for the oldest frame to keep Update/Draw from spinning when all frames are in flight.
    const uint32_t       FrameIndex = NextFrame;
    FrameConstantBuffer& ConstantBuffer = FrameConstants[FrameIndex];
    if (ConstantBuffer.Submission.IsValid())
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::waitForFrame");
        Context.Wait(ConstantBuffer.Submission);
    }
    if (!GpuSceneData.BeginFrame())
    {
        return;
    }
    NextFrame = (NextFrame + 1) % FrameConstants.Size();
    const hz::SceneView     View = { .Scene = Scene,
                                     .GpuScene = GpuSceneData,
                                     .Assets = Assets,
                                     .FrameIndex = FrameIndex,
                                     .Skinning = &Skinning };
    const hz::RenderCamera* Camera = Scene.GetCamera(CameraId);
    ASSERT(Camera);
    const hz::RenderCamera& CameraState = *Camera;
    const float             AspectInverse = (float)Context.GetHeight() / (float)Context.GetWidth();
    const TemporalJitter    Jitter =
        GTemporalAA.GetBool() ? TemporalAA::GetJitter(Shadows->Data.FrameIndex, Context.GetWidth(), Context.GetHeight()) : TemporalJitter{};
    if (GTemporalAA.GetBool() && !TemporalAAWasEnabled)
    {
        hz::InvalidateFrameGraphHistory(Graph, Targets.TemporalColor);
    }
    const Matrix4           Current = CameraViewProjection(CameraState.current, CameraState.lens, AspectInverse, Jitter);
    const FrameData         FrameValues = { .ViewProjection = Current,
                                            .PreviousViewProjection =
                                                CameraViewProjection(CameraState.previous, CameraState.previousLens, AspectInverse, PreviousJitter),
                                            .InverseViewProjection = inverse(Current),
                                            .Eye = CameraState.current.getCol3() };
    const hz::GPUBuffer&    Frame = ConstantBuffer.Buffer;
    const bool              Updated = Context.UpdateBuffer(Frame, 0, &FrameValues, sizeof(FrameValues));
    ASSERT(Updated);
    hz::CommandList& Commands = Context.AcquireCommandList();
    GpuSceneData.Upload(Commands);
    Skinning.Prepare(FrameIndex);
    Draws.Prepare(View);
    // Frame shape the passes depend on.
    const bool RayTracingReady = RayTracing->IsSupported();
    Shadows->Data.EnvironmentMipCount = Environment->GetPrefilteredMipCount();
    Shadows->Data.RTShadowEnabled = RayTracingReady && GRTShadows.GetBool() ? 1u : 0u;
    Shadows->Data.RTAOEnabled = RayTracingReady && GRTAO.GetBool() ? 1u : 0u;
    Shadows->Data.RTAOPower = RayTracing->Settings.AOPower;
    const hz::GpuSceneBuffers& SceneBuffers = GpuSceneData.GetBuffers();
    LightingFrameData&         LightingData = Shadows->Data;
    const hz::Span<uint32_t>   ActiveLights = Scene.GetActiveLights();
    LightingData.LightCount = ActiveLights.Count;
    LightingData.LightsIndex = LightingData.LightCount ? SceneBuffers.lights.GetSrvIndex() : UINT32_MAX;
    LightingData.ActiveLightsIndex = LightingData.LightCount ? SceneBuffers.activeLights.GetSrvIndex() : UINT32_MAX;
    LightingData.LuminousEfficacy = Shadows->LuminousEfficacy;
    LightingData.EnvironmentIntensity = LightingFrameData::DefaultEnvironmentIntensity;
    LightingData.EnvironmentTint = Vector4(1.0f);
    for (uint32_t LightIndex : ActiveLights)
    {
        const hz::RenderLight& Light = Scene.GetLights().Data[LightIndex];
        if (Light.ready && Light.Type == hz::LightType::Dome)
        {
            LightingData.EnvironmentIntensity = Light.intensity * exp2f(Light.exposure) * LightingData.LuminousEfficacy;
            LightingData.EnvironmentTint = Vector4(hz::ResolveLightColor(Light), 1.0f);
            break;
        }
    }
    Shadows->Prepare(View);
    const bool Clustered =
        GClusteredLighting.GetBool() && ActiveLights.Count &&
        Clusters->Prepare(FrameIndex, Scene.GetLights(), ActiveLights, LightingData.MainDirectionalIndex, Current,
                          CameraState.lens.nearPlane, CameraState.lens.farPlane, Context.GetWidth(), Context.GetHeight());
    const hz::GPUBuffer* Masks = Clustered ? Clusters->GetMask(FrameIndex) : nullptr;
    hz::SetFrameGraphBufferImport(Graph, Targets.LightMasks, Masks);
    hz::SetFrameGraphPassEnabled(Graph, LightMaskPass, GClusteredLighting.GetBool());
    // Ray traced AO replaces the screen space term.
    hz::SetFrameGraphPassEnabled(Graph, GTAOGraphPass, GGTAO.GetBool() && !Shadows->Data.RTAOEnabled);
    hz::SetFrameGraphPassEnabled(Graph, RayTracingPass,
                                 RayTracingReady &&
                                     (GRTShadows.GetBool() || GRTAO.GetBool() || GRTReflections.GetBool()));
    hz::SetFrameGraphPassEnabled(Graph, TransparencyPass, GTransparency.GetBool());
    hz::SetFrameGraphPassEnabled(Graph, TemporalAAPass, GTemporalAA.GetBool());
    const hz::GPUTexture& Backbuffer = Context.GetCurrentBackbuffer();
    hz::SetFrameGraphTextureImport(Graph, Targets.Backbuffer, &Backbuffer);
    // Stands for all of this frame's lists; passes fetch the individual lists from the draw list itself.
    hz::SetFrameGraphBufferImport(Graph, Targets.DrawLists, &Draws.GetCommands(FrameIndex, Draws.GetOpaqueList()));
    // Stands for every skinned stream; the passes fetch the streams through the draw list.
    hz::SetFrameGraphBufferImport(Graph, Targets.SkinnedVertices, &Frame);
    hz::SetFrameGraphPassEnabled(Graph, SkinningGraphPass, Skinning.GetJobCount() > 0);
    FrameInputs Inputs = { .Draws = Draws,
                           .Frame = Frame,
                           .View = View,
                           .LinearSampler = LinearSampler,
                           .Targets = Targets,
                           .Lighting = { .LightingFrame = &Shadows->GetLightingFrame(FrameIndex),
                                         .ShadowSampler = &Shadows->GetSampler(),
                                         .Environment = &Environment->GetPrefiltered(),
                                         .Irradiance = &Environment->GetIrradiance(),
                                         .BRDFLut = &Environment->GetBrdfLut(),
                                         .EnvironmentSampler = &Environment->GetSampler(),
                                         .Lights = LightingData.LightCount ? &SceneBuffers.lights : nullptr,
                                         .ActiveLights = LightingData.LightCount ? &SceneBuffers.activeLights : nullptr,
                                         .ClusterData = Clustered ? &Clusters->GetData(FrameIndex) : nullptr,
                                         .TileMasks = Masks },
                           .DeltaSeconds = DeltaSeconds,
                           .EnvironmentIntensity = Shadows->Data.EnvironmentIntensity,
                           .EnvironmentTint = Shadows->Data.EnvironmentTint,
                           .RayTracedShadows = RayTracingReady && (GRTShadows.GetBool() || GRTAO.GetBool()),
                           .RayTracedReflection = RayTracingReady && GRTReflections.GetBool(),
                           .TemporalAAEnabled = GTemporalAA.GetBool(),
                           .Jitter = Jitter,
                           .PreviousJitter = PreviousJitter };
    hz::ExecuteFrameGraph(Graph, Commands, &Inputs);
    ConstantBuffer.Submission = Context.Submit(Commands, &Backbuffer);
    GpuSceneData.EndFrame(ConstantBuffer.Submission);
    PreviousJitter = Jitter;
    TemporalAAWasEnabled = GTemporalAA.GetBool();
}
