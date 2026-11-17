#include "RenderPasses.h"
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
    uint32_t frameIndex;
    uint32_t instancesIndex;
    uint32_t materialsIndex;
    uint32_t texturesIndex;
    uint32_t samplerIndex;
};
static_assert(sizeof(GeometryBindingConstants) == 5 * sizeof(uint32_t));

GBuffer::GBuffer(hz::RenderContext& Context)
{
    Pipeline = Context.CreateGraphicsPipeline({
    .shaderDesc = {
        .stages = {
            { .stage = ShaderStageVert, .pEntryPoint = "VSIndirect", .Name = "Renderer.GeometryVS" },
            { .stage = ShaderStageFrag, .pEntryPoint = "PSMain", .Name = "Renderer.GeometryPS" },
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
            { .pTexture = Textures[0], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
            { .pTexture = Textures[1], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
            { .pTexture = Textures[2], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
            { .pTexture = Textures[3], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
        } ,
        .depthAttachment = { .pTexture = &Depth,
                             .loadAction = LoadActionClear,
                             .storeAction = StoreActionStore,
                             .ClearValue = { .Depth = 1.0f } },
    }, Draws.GetDependencies(Frame, View, &Draws.GetCommands(View.FrameIndex, Draws.GetOpaqueList())));
    Cmd.SetViewport(0, 0, (float)Width, (float)Height);
    Cmd.SetScissor(0, 0, Width, Height);
    Cmd.SetPipeline(Pipeline);
    const GeometryBindingConstants Binding = {
        .frameIndex = Frame.GetSrvIndex(),
        .instancesIndex = Gpu.instances.IsValid() ? Gpu.instances.GetSrvIndex() : UINT32_MAX,
        .materialsIndex = Gpu.materials.IsValid() ? Gpu.materials.GetSrvIndex() : UINT32_MAX,
        .texturesIndex = Gpu.textures.IsValid() ? Gpu.textures.GetSrvIndex() : UINT32_MAX,
        .samplerIndex = Sampler.GetIndex(),
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
    hz::GpuDrawList&          Draws;
    const hz::GPUBuffer&  Frame;
    const hz::SceneView&      View;
    hz::GPUSampler&       LinearSampler;
    const RenderTargets&  Targets;
    LightingResources     Lighting = {};
    float                 DeltaSeconds = 0.0f;
    float                 EnvironmentIntensity = 0.0f;
    bool                  RayTracedShadows = false;
    bool                  RayTracedReflection = false;
};

template<typename T>
struct PassData
{
    T*          Module;
    FrameInputs* pFrame = nullptr;
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

static void ExecuteCulling(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs& Frame = Inputs(Context);
    ((FrustumCulling*)UserData)->Execute(Context.Commands, Frame.Frame, Frame.View, Frame.Draws);
}

static void ExecuteShadows(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs& Frame = Inputs(Context);
    ((ShadowMap*)UserData)->Execute(Context.Commands, Frame.View, Frame.Draws, *hz::GetFrameGraphTexture(Context, Frame.Targets.ShadowMap));
}

static void ExecuteGBuffer(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs&    Frame = Inputs(Context);
    const hz::GPUTexture* Textures[GBuffer::TextureCount];
    for (uint32_t Index = 0; Index < GBuffer::TextureCount; ++Index)
        Textures[Index] = hz::GetFrameGraphTexture(Context, Frame.Targets.GBuffer[Index]);
    ((GBuffer*)UserData)->Execute(Context.Commands, Frame.Frame, Frame.View, Frame.Draws, Textures,
                                  *hz::GetFrameGraphTexture(Context, Frame.Targets.Depth));
}

static void ExecuteScreenSpaceAO(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs& Frame = Inputs(Context);
    ((ScreenSpaceAO*)UserData)
        ->Execute(Context.Commands, Frame.Frame, *hz::GetFrameGraphTexture(Context, Frame.Targets.Depth), Frame.LinearSampler,
                  *hz::GetFrameGraphTexture(Context, Frame.Targets.Occlusion),
                  *hz::GetFrameGraphTexture(Context, Frame.Targets.OcclusionScratch));
}

static void ExecuteRayTracing(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs&      Frame = Inputs(Context);
    const RenderTargets&    Targets = Frame.Targets;
    LightingResources       Lighting = Frame.Lighting;
    const RayTracedEffectsTargets RTTargets = {
        .Result = hz::GetFrameGraphTexture(Context, Targets.RTResult),
        .Scratch = hz::GetFrameGraphTexture(Context, Targets.RTScratch),
        .Reflection = hz::GetFrameGraphTexture(Context, Targets.RTReflection),
        .History = hz::GetFrameGraphTexture(Context, Targets.RTHistory),
        .PreviousHistory = hz::GetFrameGraphHistory(Context, Targets.RTHistory),
        .PreviousDepth = hz::GetFrameGraphTexture(Context, Targets.RTPreviousDepth),
        .PreviousFrameDepth = hz::GetFrameGraphHistory(Context, Targets.RTPreviousDepth),
        .HistoryValid = hz::IsFrameGraphHistoryValid(Context, Targets.RTHistory) &&
                        hz::IsFrameGraphHistoryValid(Context, Targets.RTPreviousDepth),
    };
    ((RayTracedEffects*)UserData)->Execute(Context.Commands, Frame.Frame, GetGBuffer(Context), Frame.View, Frame.Draws, Lighting, RTTargets);
}

static void ExecuteLighting(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs&   Frame = Inputs(Context);
    const RenderTargets& Targets = Frame.Targets;
    LightingResources    Lighting = Frame.Lighting;
    Lighting.ShadowMap = hz::GetFrameGraphTexture(Context, Targets.ShadowMap);
    Lighting.SSAO = hz::GetFrameGraphTexture(Context, Targets.Occlusion);
    // The pass writes both images whenever it runs; hand over only the effects that are enabled, as the shader expects.
    Lighting.RTShadowAO = Frame.RayTracedShadows ? hz::GetFrameGraphTexture(Context, Targets.RTResult) : nullptr;
    Lighting.RTReflection = Frame.RayTracedReflection ? hz::GetFrameGraphTexture(Context, Targets.RTReflection) : nullptr;
    ((DeferredLighting*)UserData)
        ->Execute(Context.Commands, Frame.Frame, GetGBuffer(Context), Lighting, *hz::GetFrameGraphTexture(Context, Targets.SceneColor));
}

static void ExecuteEnvironment(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs& Frame = Inputs(Context);
    ((EnvironmentPass*)UserData)
        ->Execute(Context.Commands, Frame.Frame, *hz::GetFrameGraphTexture(Context, Frame.Targets.SceneColor),
                  *hz::GetFrameGraphTexture(Context, Frame.Targets.Depth), Frame.EnvironmentIntensity);
}

static void ExecuteTransparency(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs&   Frame = Inputs(Context);
    const RenderTargets& Targets = Frame.Targets;
    LightingResources    Lighting = Frame.Lighting;
    Lighting.ShadowMap = hz::GetFrameGraphTexture(Context, Targets.ShadowMap);
    const TransparencyTargets OITTargets = { .Heads = hz::GetFrameGraphTexture(Context, Targets.OITHeads),
                                             .Color = hz::GetFrameGraphTexture(Context, Targets.TransparentColor),
                                             .Nodes = hz::GetFrameGraphBuffer(Context, Targets.OITNodes),
                                             .Counters = hz::GetFrameGraphBuffer(Context, Targets.OITCounters) };
    ((Transparency*)UserData)
        ->Execute(Context.Commands, Frame.Frame, *hz::GetFrameGraphTexture(Context, Targets.Depth), Frame.View, Frame.Draws, Lighting,
                  *hz::GetFrameGraphTexture(Context, Targets.SceneColor), OITTargets);
}

static void ExecutePost(const hz::FrameGraphContext& Context, void* UserData)
{
    const FrameInputs&   Frame = Inputs(Context);
    const RenderTargets& Targets = Frame.Targets;
    PostEffectsTargets   PostTargets = { .PreviousAdapted = hz::GetFrameGraphHistory(Context, Targets.AdaptedLuminance),
                                         .Adapted = hz::GetFrameGraphTexture(Context, Targets.AdaptedLuminance),
                                         .AdaptedValid = hz::IsFrameGraphHistoryValid(Context, Targets.AdaptedLuminance) };
    for (uint32_t Level = 0; Level < PostEffectsTargets::LuminanceLevels; ++Level)
        PostTargets.Luminance[Level] = hz::GetFrameGraphTexture(Context, Targets.Luminance[Level]);
    for (uint32_t Level = 0; Level < PostEffectsTargets::BloomLevels; ++Level)
        PostTargets.BloomDown[Level] = hz::GetFrameGraphTexture(Context, Targets.BloomDown[Level]);
    for (uint32_t Level = 0; Level + 1 < PostEffectsTargets::BloomLevels; ++Level)
        PostTargets.BloomUp[Level] = hz::GetFrameGraphTexture(Context, Targets.BloomUp[Level]);
    // Transparency resolves over the opaque image into its own target; without it, post reads the opaque image directly.
    const hz::GPUTexture* Transparent = hz::GetFrameGraphTexture(Context, Targets.TransparentColor);
    const hz::GPUTexture& SceneColor = Transparent ? *Transparent : *hz::GetFrameGraphTexture(Context, Targets.SceneColor);
    ((PostEffects*)UserData)
        ->Execute(Context.Commands, SceneColor, *hz::GetFrameGraphTexture(Context, Targets.Backbuffer), Frame.DeltaSeconds, PostTargets);
}

static hz::FrameGraphTexture ViewportTexture(hz::FrameGraph* Graph, hz::Format Format, const char* Name, bool RenderTarget,
                                             uint32_t SizeShift = 0, bool History = false)
{
    const DescriptorType Storage = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture);
    return hz::AddFrameGraphTexture(Graph, { .Texture = { .Format = Format,
                                                          .StartState = RenderTarget ? ResourceStateRenderTarget : ResourceStateUnorderedAccess,
                                                          .Descriptors = RenderTarget ? DescriptorTypeTexture : Storage,
                                                          .Flags = TextureCreationFlagForce2D,
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
    hz::FrameGraph* G = Graph;
    RenderTargets&  T = Targets;
    for (uint32_t Index = 0; Index < GBuffer::TextureCount; ++Index)
        T.GBuffer[Index] = ViewportTexture(G, GBufferFormats[Index], GBufferNames[Index], true);
    T.Depth = hz::AddFrameGraphTexture(G, { .Texture = { .Format = DepthFormat,
                                                         .StartState = ResourceStateDepthWrite,
                                                         .Descriptors = DescriptorTypeTexture,
                                                         .Flags = TextureCreationFlagForce2D,
                                                         .GetRenderTarget = true,
                                                         .Name = "Renderer.Depth" } });
    T.ShadowMap = hz::AddFrameGraphTexture(G, { .Texture = { .Width = ShadowMap::Resolution,
                                                             .Height = ShadowMap::Resolution,
                                                             .Format = ShadowMap::DepthFormat,
                                                             .StartState = ResourceStateDepthWrite,
                                                             .Descriptors = DescriptorTypeTexture,
                                                             .GetRenderTarget = true,
                                                             .Name = "Renderer.ShadowMap" } });
    T.Occlusion = ViewportTexture(G, ScreenSpaceAO::TargetFormat, "Renderer.SSAO", false);
    T.OcclusionScratch = ViewportTexture(G, ScreenSpaceAO::TargetFormat, "Renderer.SSAO.Scratch", false);
    T.RTResult = ViewportTexture(G, RayTracedEffects::ResultFormat, "Renderer.RT.ShadowAO", false);
    T.RTScratch = ViewportTexture(G, RayTracedEffects::ScratchFormat, "Renderer.RT.DenoiseScratch", false);
    T.RTReflection = ViewportTexture(G, RayTracedEffects::ReflectionFormat, "Renderer.RT.Reflection", false);
    T.RTHistory = ViewportTexture(G, RayTracedEffects::HistoryFormat, "Renderer.RT.History", false, 0, true);
    T.RTPreviousDepth = ViewportTexture(G, RayTracedEffects::DepthFormat, "Renderer.RT.PreviousDepth", false, 0, true);
    T.SceneColor = ViewportTexture(G, DeferredLighting::ColorFormat, "Renderer.DeferredLighting.Color", true);
    T.OITHeads = ViewportTexture(G, Transparency::HeadsFormat, "Renderer.OIT.Heads", false);
    T.TransparentColor = ViewportTexture(G, Transparency::ColorFormat, "Renderer.OIT.Resolved", false);
    const DescriptorType RawStorage = (DescriptorType)(DescriptorTypeBufferRaw | DescriptorTypeRwBufferRaw);
    // One fragment per pixel on average, as before.
    T.OITNodes = hz::AddFrameGraphBuffer(G, { .Buffer = { .StructStride = 4,
                                                          .Name = "Renderer.OIT.Nodes",
                                                          .usage = MemoryUsageGpuOnly,
                                                          .StartState = ResourceStateUnorderedAccess,
                                                          .Descriptors = RawStorage },
                                              .BytesPerPixel = Transparency::NodeBytes });
    T.OITCounters = hz::AddFrameGraphBuffer(G, { .Buffer = { .Size = 16,
                                                             .ElementCount = 4,
                                                             .Name = "Renderer.OIT.Counters",
                                                             .usage = MemoryUsageGpuOnly,
                                                             .StartState = ResourceStateUnorderedAccess,
                                                             .Descriptors = RawStorage } });
    const DescriptorType Storage = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture);
    for (uint32_t Level = 0; Level < PostEffectsTargets::LuminanceLevels; ++Level)
    {
        const uint32_t Size = PostEffectsTargets::MeteringSize >> Level;
        T.Luminance[Level] = hz::AddFrameGraphTexture(G, { .Texture = { .Width = Size ? Size : 1,
                                                                        .Height = Size ? Size : 1,
                                                                        .Format = PostEffects::LuminanceFormat,
                                                                        .StartState = ResourceStateUnorderedAccess,
                                                                        .Descriptors = Storage,
                                                                        .Flags = TextureCreationFlagForce2D,
                                                                        .Name = "Renderer.HDR.LogLuminance" } });
    }
    // Bloom follows the render size so the glare keeps its screen-space radius; level 0 is half the viewport.
    for (uint32_t Level = 0; Level < PostEffectsTargets::BloomLevels; ++Level)
    {
        T.BloomDown[Level] = ViewportTexture(G, PostEffects::BloomFormat, "Renderer.HDR.BloomDown", false, Level + 1);
        if (Level + 1 < PostEffectsTargets::BloomLevels)
            T.BloomUp[Level] = ViewportTexture(G, PostEffects::BloomFormat, "Renderer.HDR.BloomUp", false, Level + 1);
    }
    T.AdaptedLuminance = hz::AddFrameGraphTexture(G, { .Texture = { .Width = 1,
                                                                    .Height = 1,
                                                                    .Format = PostEffects::AdaptedFormat,
                                                                    .StartState = ResourceStateUnorderedAccess,
                                                                    .Descriptors = Storage,
                                                                    .Flags = TextureCreationFlagForce2D,
                                                                    .Name = "Renderer.HDR.AdaptedLuminance" },
                                                       .History = true });
    T.Backbuffer = hz::ImportFrameGraphTexture(G, { .Name = "Backbuffer", .Output = true });
    // GpuDrawList recreates its list buffers as the scene grows, so they stay outside the graph's resources; the import
    // only orders culling before the passes that draw from the lists.
    T.DrawLists = hz::ImportFrameGraphBuffer(G, { .Name = "DrawLists" });

    const hz::FrameGraphBufferUse WriteLists[] = { { T.DrawLists, hz::FrameGraphAccessWrite } };
    const hz::FrameGraphBufferUse ReadLists[] = { { T.DrawLists, hz::FrameGraphAccessRead } };
    hz::AddFrameGraphPass(G, { .Name = "Culling", .Execute = ExecuteCulling, .UserData = Culling.Get(), .Buffers = WriteLists });
    const Use ShadowUses[] = { { T.ShadowMap, Write } };
    hz::AddFrameGraphPass(G,
                          { .Name = "Shadows", .Execute = ExecuteShadows, .UserData = Shadows.Get(), .Textures = ShadowUses, .Buffers = ReadLists });
    const Use GBufferUses[] = { { T.GBuffer[0], Write }, { T.GBuffer[1], Write }, { T.GBuffer[2], Write },
                                { T.GBuffer[3], Write }, { T.Depth, Write } };
    hz::AddFrameGraphPass(
        G, { .Name = "GBuffer", .Execute = ExecuteGBuffer, .UserData = GBufferPass.Get(), .Textures = GBufferUses, .Buffers = ReadLists });
    const Use SSAOUses[] = { { T.Depth, Read }, { T.Occlusion, Write }, { T.OcclusionScratch, Write } };
    SSAOPass = hz::AddFrameGraphPass(G, { .Name = "SSAO", .Execute = ExecuteScreenSpaceAO, .UserData = SSAO.Get(), .Textures = SSAOUses });
    const Use RTUses[] = { { T.GBuffer[1], Read },           { T.Depth, Read },          { T.RTResult, Write },
                           { T.RTScratch, Write },           { T.RTReflection, Write },  { T.RTHistory, History },
                           { T.RTHistory, Write },           { T.RTPreviousDepth, History }, { T.RTPreviousDepth, Write } };
    RayTracingPass =
        hz::AddFrameGraphPass(G, { .Name = "Ray Tracing", .Execute = ExecuteRayTracing, .UserData = RayTracing.Get(), .Textures = RTUses });
    const Use LightingUses[] = { { T.GBuffer[0], Read }, { T.GBuffer[1], Read },          { T.GBuffer[2], Read },
                                 { T.Depth, Read },      { T.ShadowMap, Read },           { T.Occlusion, Optional },
                                 { T.RTResult, Optional }, { T.RTReflection, Optional },   { T.SceneColor, Write } };
    hz::AddFrameGraphPass(G, { .Name = "Lighting", .Execute = ExecuteLighting, .UserData = Lighting.Get(), .Textures = LightingUses });
    const Use EnvironmentUses[] = { { T.SceneColor, Modify }, { T.Depth, Read } };
    hz::AddFrameGraphPass(G, { .Name = "Environment", .Execute = ExecuteEnvironment, .UserData = Environment.Get(), .Textures = EnvironmentUses });
    const Use TransparencyUses[] = { { T.Depth, Read },     { T.ShadowMap, Read },        { T.SceneColor, Read },
                                     { T.OITHeads, Write }, { T.TransparentColor, Write } };
    const hz::FrameGraphBufferUse TransparencyBuffers[] = { { T.OITNodes, Write }, { T.OITCounters, Write } };
    const hz::FrameGraphBufferUse TransparencyAllBuffers[] = { TransparencyBuffers[0], TransparencyBuffers[1], ReadLists[0] };
    TransparencyPass = hz::AddFrameGraphPass(G, { .Name = "Transparency",
                                                  .Execute = ExecuteTransparency,
                                                  .UserData = Transparency.Get(),
                                                  .Textures = TransparencyUses,
                                                  .Buffers = TransparencyAllBuffers });
    Use PostUses[3 + PostEffectsTargets::LuminanceLevels + 2 * PostEffectsTargets::BloomLevels - 1 + 2];
    uint32_t PostUseCount = 0;
    PostUses[PostUseCount++] = { T.SceneColor, Read };
    PostUses[PostUseCount++] = { T.TransparentColor, Optional };
    PostUses[PostUseCount++] = { T.Backbuffer, Write };
    for (hz::FrameGraphTexture Texture : T.Luminance)
        PostUses[PostUseCount++] = { Texture, Write };
    for (hz::FrameGraphTexture Texture : T.BloomDown)
        PostUses[PostUseCount++] = { Texture, Write };
    for (hz::FrameGraphTexture Texture : T.BloomUp)
        PostUses[PostUseCount++] = { Texture, Write };
    PostUses[PostUseCount++] = { T.AdaptedLuminance, History };
    PostUses[PostUseCount++] = { T.AdaptedLuminance, Write };
    ASSERT(PostUseCount == TF_ARRAY_COUNT(PostUses));
    hz::AddFrameGraphPass(G, { .Name = "Post", .Execute = ExecutePost, .UserData = PostProcessing.Get(), .Textures = PostUses });
}

RenderPasses::RenderPasses(const RenderPassesDesc& Desc):
    Context(*Desc.Context), Scene(*Desc.Scene), Assets(*Desc.AssetManager), CameraId(Desc.Camera), Draws(Context),
    GpuSceneData(Scene, Assets, Context, { .frameCount = FrameConstants.Size() })
{
    ASSERT(Context.IsValid() && CameraId.IsValid());
    Culling = hz::MakeUnique<FrustumCulling>(Context);
    GBufferPass = hz::MakeUnique<GBuffer>(Context);
    Lighting = hz::MakeUnique<DeferredLighting>(Context);
    SSAO = hz::MakeUnique<ScreenSpaceAO>(Context);
    Environment = hz::MakeUnique<EnvironmentPass>(Context, DeferredLighting::ColorFormat);
    Shadows = hz::MakeUnique<ShadowMap>(Context);
    PostProcessing = hz::MakeUnique<PostEffects>(Context);
    Transparency = hz::MakeUnique<::Transparency>(Context);
    RayTracing = hz::MakeUnique<RayTracedEffects>(Context);
    Graph = hz::InitFrameGraph(&Context);
    BuildGraph();

    LinearSampler = Context.CreateSampler({ .AddressU = AddressModeClampToEdge, .AddressV = AddressModeClampToEdge });
    for (FrameConstantBuffer& ConstantBuffer : FrameConstants)
    {
        hz::GPUBuffer& Buffer = ConstantBuffer.Buffer;
        Buffer = Context.CreateBuffer({ .Size = sizeof(FrameData),
                                        .ElementCount = 1,
                                        .StructStride = sizeof(FrameData),
                                        .Name = "Renderer.Frame",
                                        .usage = MemoryUsageCpuToGpu,
                                        .StartState = ResourceStateShaderResource,
                                        .Descriptors = DescriptorTypeBuffer,
                                        .Flags = BufferCreationFlagPersistentMapBit });
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
    if (!SSAO->Load() || !RayTracing->Load())
    {
        return false;
    }
    PostProcessing->Resize();
    if (!hz::ResizeFrameGraph(Graph, Width, Height))
    {
        LOGF(Error, "Cannot compile the frame graph for %ux%u", Width, Height);
        return false;
    }

    Scene.markDirty();
    return true;
}

void RenderPasses::Update(float FrameDeltaSeconds) { DeltaSeconds = FrameDeltaSeconds; }

static Matrix4 CameraViewProjection(const Matrix4& Pose, const hz::Camera& Lens, float AspectInverse)
{
    const float HorizontalFov = 2.0f * atanf(tanf(Lens.verticalFov * 0.5f) / AspectInverse);
    return Matrix4::perspectiveRH(HorizontalFov, AspectInverse, Lens.nearPlane, Lens.farPlane) * inverse(Pose);
}

void RenderPasses::Execute()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::execute");
    if (Context.IsSuspended())
    {
        return;
    }

    ASSERT(PostProcessing);
    // Block on the oldest frame like Unreal's frame sync; returning would spin Update/Draw while every frame is in flight.
    const uint32_t     FrameIndex = NextFrame;
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
    const hz::SceneView         View = { .Scene = Scene, .GpuScene = GpuSceneData, .Assets = Assets, .FrameIndex = FrameIndex };
    const hz::RenderCamera* Camera = Scene.GetCamera(CameraId);
    ASSERT(Camera);
    const hz::RenderCamera& CameraState = *Camera;
    const float             AspectInverse = (float)Context.GetHeight() / (float)Context.GetWidth();
    const Matrix4           Current = CameraViewProjection(CameraState.current, CameraState.lens, AspectInverse);
    const FrameData         FrameValues = { .viewProjection = Current,
                                            .previousViewProjection =
                                                CameraViewProjection(CameraState.previous, CameraState.previousLens, AspectInverse),
                                            .inverseViewProjection = inverse(Current),
                                            .eye = CameraState.current.getCol3() };
    const hz::GPUBuffer&    Frame = ConstantBuffer.Buffer;
    const bool              Updated = Context.UpdateBuffer(Frame, 0, &FrameValues, sizeof(FrameValues));
    ASSERT(Updated);
    hz::CommandList& Commands = Context.AcquireCommandList();
    GpuSceneData.Upload(Commands);
    Draws.Prepare(View);
    // Frame shape the passes depend on.
    const bool RayTracingReady = RayTracing->IsSupported();
    Shadows->Data.environmentMipCount = Environment->GetPrefilteredMipCount();
    Shadows->Data.rtShadowEnabled = RayTracingReady && RayTracing->Settings.Shadows ? 1u : 0u;
    Shadows->Data.rtAOEnabled = RayTracingReady && RayTracing->Settings.AO ? 1u : 0u;
    Shadows->Data.rtAOPower = RayTracing->Settings.AOPower;
    Shadows->Prepare(View);
    // Ray traced AO replaces the screen space term.
    hz::SetFrameGraphPassEnabled(Graph, SSAOPass, EnableScreenSpaceAO && !Shadows->Data.rtAOEnabled);
    hz::SetFrameGraphPassEnabled(Graph, RayTracingPass,
                                 RayTracingReady && (RayTracing->Settings.Shadows || RayTracing->Settings.AO || RayTracing->Settings.Reflection));
    hz::SetFrameGraphPassEnabled(Graph, TransparencyPass, EnableTransparency);
    const hz::GPUTexture& Backbuffer = Context.GetCurrentBackbuffer();
    hz::SetFrameGraphTextureImport(Graph, Targets.Backbuffer, &Backbuffer);
    // Stands for all of this frame's lists; passes fetch the individual lists from the draw list itself.
    hz::SetFrameGraphBufferImport(Graph, Targets.DrawLists, &Draws.GetCommands(FrameIndex, Draws.GetOpaqueList()));
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
                                         .EnvironmentSampler = &Environment->GetSampler() },
                           .DeltaSeconds = DeltaSeconds,
                           .EnvironmentIntensity = Shadows->Data.environmentIntensity,
                           .RayTracedShadows = RayTracingReady && (RayTracing->Settings.Shadows || RayTracing->Settings.AO),
                           .RayTracedReflection = RayTracingReady && RayTracing->Settings.Reflection };
    hz::ExecuteFrameGraph(Graph, Commands, &Inputs);
    ConstantBuffer.Submission = Context.Submit(Commands, &Backbuffer);
    GpuSceneData.EndFrame(ConstantBuffer.Submission);
}
