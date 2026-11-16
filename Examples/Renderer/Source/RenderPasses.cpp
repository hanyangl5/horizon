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

GBuffer::GBuffer(hz::RenderContext& Context): Context(Context)
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

void GBuffer::Resize(uint32_t Width, uint32_t Height)
{
    this->Width = Width;
    this->Height = Height;
    hz::TextureDesc TargetDesc = {
        .Width = Width,
        .Height = Height,
        .StartState = ResourceStateRenderTarget,
        .Descriptors = DescriptorTypeTexture,
        .Flags = TextureCreationFlagForce2D,
        .renderTarget = true,
    };
    for (uint32_t Index = 0; Index < TextureCount; ++Index)
    {
        TargetDesc.Format = GBufferFormats[Index];
        TargetDesc.Name = GBufferNames[Index];
        Textures[Index] = Context.CreateTexture(TargetDesc);
    }

    TargetDesc.Format = DepthFormat;
    TargetDesc.StartState = ResourceStateDepthWrite;
    TargetDesc.Name = "Renderer.Depth";
    Depth = Context.CreateTexture(TargetDesc);
}

void GBuffer::Execute(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const SceneView& View, const GpuDrawList& Draws)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GBuffer::execute");
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.getBuffers();
    const ClearValue           Black = {};
    Cmd.BeginGpuTimestamp("Geometry");
    Cmd.BeginRendering({
        .colorAttachments =  {
            { .pTexture = &Textures[0], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
            { .pTexture = &Textures[1], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
            { .pTexture = &Textures[2], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
            { .pTexture = &Textures[3], .loadAction = LoadActionClear, .storeAction = StoreActionStore, .ClearValue = Black },
        } ,
        .depthAttachment = { .pTexture = &Depth,
                             .loadAction = LoadActionClear,
                             .storeAction = StoreActionStore,
                             .ClearValue = { .Depth = 1.0f } },
    }, Draws.GetDependencies(Frame, View, &Draws.GetCommands(View.FrameIndex)));
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
        Cmd.DrawIndirect(Draws.GetCommands(View.FrameIndex), 0, Draws.GetCount(View.FrameIndex));
    }
    Cmd.EndRendering();
    Cmd.EndGpuTimestamp();
}

static void ExecuteShadows(FrameContext& Frame, void* UserData)
{
    ShadowMap& Shadows = *(ShadowMap*)UserData;
    Shadows.Execute(Frame.Commands, Frame.View, Frame.Draws);
    Frame.EnvironmentIntensity = Shadows.Data.environmentIntensity;
}

static void ExecuteGBuffer(FrameContext& Frame, void* UserData)
{
    GBuffer& Pass = *(GBuffer*)UserData;
    Pass.Execute(Frame.Commands, Frame.Frame, Frame.View, Frame.Draws);
    Frame.GBuffer = { .Emissive = &Pass.Textures[0],
                      .NormalMaterial = &Pass.Textures[1],
                      .BaseColorMetallic = &Pass.Textures[2],
                      .Depth = &Pass.Depth };
}

static void ExecuteScreenSpaceAO(FrameContext& Frame, void* UserData)
{
    // Ray traced AO replaces the screen space term.
    if (!Frame.RayTracedAO)
    {
        Frame.Resources.SSAO =
            &((ScreenSpaceAO*)UserData)->Execute(Frame.Commands, Frame.Frame, *Frame.GBuffer.Depth, Frame.LinearSampler);
    }
}

static void ExecuteRayTracing(FrameContext& Frame, void* UserData)
{
    RayTracedEffects& RayTracing = *(RayTracedEffects*)UserData;
    RayTracing.Execute(Frame.Commands, Frame.Frame, Frame.GBuffer, Frame.View, Frame.Draws, Frame.Resources);
    if (!RayTracing.IsSupported())
    {
        return;
    }
    if (RayTracing.Settings.Shadows || RayTracing.Settings.AO)
    {
        Frame.Resources.RTShadowAO = &RayTracing.GetShadowAO();
    }
    if (RayTracing.Settings.Reflection)
    {
        Frame.Resources.RTReflection = &RayTracing.GetReflection();
    }
}

static void ExecuteLighting(FrameContext& Frame, void* UserData)
{
    Frame.Color = &((DeferredLighting*)UserData)->Execute(Frame.Commands, Frame.Frame, Frame.GBuffer, Frame.View, Frame.Resources);
}

static void ExecuteEnvironment(FrameContext& Frame, void* UserData)
{
    ((EnvironmentPass*)UserData)->Execute(Frame.Commands, Frame.Frame, *Frame.Color, *Frame.GBuffer.Depth, Frame.EnvironmentIntensity);
}

static void ExecuteTransparency(FrameContext& Frame, void* UserData)
{
    Frame.Color = &((Transparency*)UserData)
                       ->Execute(Frame.Commands, Frame.Frame, *Frame.GBuffer.Depth, Frame.View, Frame.Draws, Frame.Resources,
                                 *Frame.Color);
}

static void ExecutePost(FrameContext& Frame, void* UserData)
{
    ((PostEffects*)UserData)->Execute(Frame.Commands, *Frame.Color, *Frame.Backbuffer, Frame.DeltaSeconds);
}

RenderPasses::RenderPasses(const RenderPassesDesc& Desc):
    Context(*Desc.Context), Scene(*Desc.Scene), Assets(*Desc.AssetManager), CameraId(Desc.Camera), Draws(Context),
    GpuSceneData(Scene, Assets, Context, { .frameCount = FrameConstants.Size() })
{
    ASSERT(Context.IsValid() && CameraId.IsValid());
    GBufferTargets = hz::MakeUnique<GBuffer>(Context);
    Lighting = hz::MakeUnique<DeferredLighting>(Context);
    SSAO = hz::MakeUnique<ScreenSpaceAO>(Context);
    Environment = hz::MakeUnique<EnvironmentPass>(Context, hz::Format::R32G32B32A32_SFLOAT);
    Shadows = hz::MakeUnique<ShadowMap>(Context);
    PostProcessing = hz::MakeUnique<PostEffects>(Context);
    Transparency = hz::MakeUnique<::Transparency>(Context);
    RayTracing = hz::MakeUnique<RayTracedEffects>(Context);
    AddPass({ .Name = "Shadows", .Execute = ExecuteShadows, .UserData = Shadows.Get() });
    AddPass({ .Name = "GBuffer", .Execute = ExecuteGBuffer, .UserData = GBufferTargets.Get() });
    AddPass({ .Name = "SSAO", .Execute = ExecuteScreenSpaceAO, .UserData = SSAO.Get(), .Enabled = &EnableScreenSpaceAO });
    AddPass({ .Name = "Ray Tracing", .Execute = ExecuteRayTracing, .UserData = RayTracing.Get() });
    AddPass({ .Name = "Lighting", .Execute = ExecuteLighting, .UserData = Lighting.Get() });
    AddPass({ .Name = "Environment", .Execute = ExecuteEnvironment, .UserData = Environment.Get() });
    AddPass({ .Name = "Transparency", .Execute = ExecuteTransparency, .UserData = Transparency.Get(), .Enabled = &EnableTransparency });
    AddPass({ .Name = "Post", .Execute = ExecutePost, .UserData = PostProcessing.Get() });

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
    if (!SSAO->Resize(Width, Height))
    {
        return false;
    }
    GBufferTargets->Resize(Width, Height);
    Lighting->Resize(Width, Height);
    Transparency->Resize(Width, Height, 1);
    RayTracing->Resize(Width, Height);
    PostProcessing->Resize(Width, Height);

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
    if (!GpuSceneData.beginFrame())
    {
        return;
    }
    NextFrame = (NextFrame + 1) % FrameConstants.Size();
    const SceneView         View = { .Scene = Scene, .GpuScene = GpuSceneData, .Assets = Assets, .FrameIndex = FrameIndex };
    const hz::RenderCamera* Camera = Scene.getCamera(CameraId);
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
    GpuSceneData.upload(Commands);
    Draws.Prepare(Commands, Frame, View);
    // Frame shape the passes depend on; the shadow pass publishes its intensity once it ran.
    Shadows->Data.environmentMipCount = Environment->GetPrefilteredMipCount();
    Shadows->Data.rtShadowEnabled = RayTracing->IsSupported() && RayTracing->Settings.Shadows ? 1u : 0u;
    Shadows->Data.rtAOEnabled = RayTracing->IsSupported() && RayTracing->Settings.AO ? 1u : 0u;
    const hz::GPUTexture& Backbuffer = Context.GetCurrentBackbuffer();
    FrameContext          FrameState = { .Commands = Commands,
                                         .Draws = Draws,
                                         .Frame = Frame,
                                         .View = View,
                                         .LinearSampler = LinearSampler,
                                         .DeltaSeconds = DeltaSeconds,
                                         .RayTracedAO = Shadows->Data.rtAOEnabled != 0,
                                         .Backbuffer = &Backbuffer,
                                         .Resources = { .LightingFrame = &Shadows->GetLightingFrame(FrameIndex),
                                                        .ShadowMap = &Shadows->GetTexture(),
                                                        .ShadowSampler = &Shadows->GetSampler(),
                                                        .Environment = &Environment->GetPrefiltered(),
                                                        .Irradiance = &Environment->GetIrradiance(),
                                                        .BRDFLut = &Environment->GetBrdfLut(),
                                                        .EnvironmentSampler = &Environment->GetSampler() } };
    for (uint32_t Index = 0; Index < PassCount; ++Index)
    {
        const RenderPass& Pass = Passes[Index];
        if (Pass.Enabled && !*Pass.Enabled)
            continue;
        Pass.Execute(FrameState, Pass.UserData);
    }
    ASSERT(FrameState.Color && FrameState.Backbuffer);
    ConstantBuffer.Submission = Context.Submit(Commands, FrameState.Backbuffer);
    GpuSceneData.endFrame(ConstantBuffer.Submission);
}
