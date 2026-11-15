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
    Pipeline = Context.createGraphicsPipeline({
    .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect", .pName = "Renderer.GeometryVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.GeometryPS" },
        },
        .pFileName = "Geometry.slang",
    },
    .depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
    .colorTargets = {
        { .format = GBufferFormats[0] },
        { .format = GBufferFormats[1] },
        { .format = GBufferFormats[2] },
        { .format = GBufferFormats[3] },
    },
    .depthStencilFormat = DepthFormat,
    .indirectDrawId = true,
    .pName = "Renderer.GeometryPipeline",
});

    Sampler = Context.createSampler();
    ASSERT(Pipeline.isValid() && Sampler.isValid());
}

void GBuffer::Resize(uint32_t Width, uint32_t Height)
{
    this->Width = Width;
    this->Height = Height;
    hz::TextureDesc TargetDesc = {
        .width = Width,
        .height = Height,
        .startState = RESOURCE_STATE_RENDER_TARGET,
        .descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
        .renderTarget = true,
    };
    for (uint32_t Index = 0; Index < TextureCount; ++Index)
    {
        TargetDesc.format = GBufferFormats[Index];
        TargetDesc.pName = GBufferNames[Index];
        Textures[Index] = Context.createTexture(TargetDesc);
    }

    TargetDesc.format = DepthFormat;
    TargetDesc.startState = RESOURCE_STATE_DEPTH_WRITE;
    TargetDesc.pName = "Renderer.Depth";
    Depth = Context.createTexture(TargetDesc);
}

void GBuffer::Execute(hz::CommandList& Cmd, const hz::GPUBuffer& Frame, const SceneView& View, const GpuDrawList& Draws)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GBuffer::Execute");
    const hz::GpuSceneBuffers& Gpu = View.gpuScene.getBuffers();
    const ClearValue           Black = {};
    Cmd.beginGpuTimestamp("Geometry");
    Cmd.beginRendering({
        .colorAttachments =  {
            { .pTexture = &Textures[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = Black },
            { .pTexture = &Textures[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = Black },
            { .pTexture = &Textures[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = Black },
            { .pTexture = &Textures[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = Black },
        } ,
        .depthAttachment = { .pTexture = &Depth,
                             .loadAction = LOAD_ACTION_CLEAR,
                             .storeAction = STORE_ACTION_STORE,
                             .clearValue = { .depth = 1.0f } },
    }, Draws.GetDependencies(Frame, View, &Draws.GetCommands(View.frameIndex)));
    Cmd.setViewport(0, 0, (float)Width, (float)Height);
    Cmd.setScissor(0, 0, Width, Height);
    Cmd.setPipeline(Pipeline);
    const GeometryBindingConstants Binding = {
        .frameIndex = Frame.getSrvIndex(),
        .instancesIndex = Gpu.instances.isValid() ? Gpu.instances.getSrvIndex() : UINT32_MAX,
        .materialsIndex = Gpu.materials.isValid() ? Gpu.materials.getSrvIndex() : UINT32_MAX,
        .texturesIndex = Gpu.textures.isValid() ? Gpu.textures.getSrvIndex() : UINT32_MAX,
        .samplerIndex = Sampler.getIndex(),
    };
    Cmd.setRootConstants(Binding, GeometryBindingSlot);

    Cmd.setRootConstant({ Draws.GetDrawData(View.frameIndex).getSrvIndex() }, GeometryDrawSlot);
    if (Draws.GetCount(View.frameIndex))
    {
        Cmd.drawIndirect(Draws.GetCommands(View.frameIndex), 0, Draws.GetCount(View.frameIndex));
    }
    Cmd.endRendering();
    Cmd.endGpuTimestamp();
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
    GpuSceneData(Scene, Assets, Context, { .frameCount = FrameConstants.size() })
{
    ASSERT(Context.isValid() && CameraId.isValid());
    GBufferTargets = hz::make_unique<GBuffer>(Context);
    Lighting = hz::make_unique<DeferredLighting>(Context);
    SSAO = hz::make_unique<ScreenSpaceAO>(Context);
    Environment = hz::make_unique<EnvironmentPass>(Context, hz::Format::R32G32B32A32_SFLOAT);
    Shadows = hz::make_unique<ShadowMap>(Context);
    PostProcessing = hz::make_unique<PostEffects>(Context);
    Transparency = hz::make_unique<::Transparency>(Context);
    RayTracing = hz::make_unique<RayTracedEffects>(Context);
    AddPass({ .Name = "Shadows", .Execute = ExecuteShadows, .UserData = Shadows.get() });
    AddPass({ .Name = "GBuffer", .Execute = ExecuteGBuffer, .UserData = GBufferTargets.get() });
    AddPass({ .Name = "SSAO", .Execute = ExecuteScreenSpaceAO, .UserData = SSAO.get(), .Enabled = &EnableScreenSpaceAO });
    AddPass({ .Name = "Ray Tracing", .Execute = ExecuteRayTracing, .UserData = RayTracing.get() });
    AddPass({ .Name = "Lighting", .Execute = ExecuteLighting, .UserData = Lighting.get() });
    AddPass({ .Name = "Environment", .Execute = ExecuteEnvironment, .UserData = Environment.get() });
    AddPass({ .Name = "Transparency", .Execute = ExecuteTransparency, .UserData = Transparency.get(), .Enabled = &EnableTransparency });
    AddPass({ .Name = "Post", .Execute = ExecutePost, .UserData = PostProcessing.get() });

    LinearSampler = Context.createSampler({ .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE });
    for (FrameConstantBuffer& ConstantBuffer : FrameConstants)
    {
        hz::GPUBuffer& Buffer = ConstantBuffer.Buffer;
        Buffer = Context.createBuffer({ .size = sizeof(FrameData),
                                        .elementCount = 1,
                                        .structStride = sizeof(FrameData),
                                        .pName = "Renderer.Frame",
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .descriptors = DESCRIPTOR_TYPE_BUFFER,
                                        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(Buffer.isValid());
    }
}

bool RenderPasses::Resize(uint32_t Width, uint32_t Height)
{
    if (!Width || !Height)
    {
        return true;
    }

    if (!Environment->IsValid() && !Environment->Load({ .Directory = RD_SHADER_SOURCES,
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
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::Execute");
    if (Context.isSuspended())
    {
        return;
    }

    ASSERT(PostProcessing);
    // Block on the oldest frame like Unreal's frame sync; returning would spin Update/Draw while every frame is in flight.
    const uint32_t     FrameIndex = NextFrame;
    FrameConstantBuffer& ConstantBuffer = FrameConstants[FrameIndex];
    if (ConstantBuffer.Submission.isValid())
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::waitForFrame");
        Context.wait(ConstantBuffer.Submission);
    }
    if (!GpuSceneData.beginFrame())
    {
        return;
    }
    NextFrame = (NextFrame + 1) % FrameConstants.size();
    const SceneView         View = { .scene = Scene, .gpuScene = GpuSceneData, .assets = Assets, .frameIndex = FrameIndex };
    const hz::RenderCamera* Camera = Scene.getCamera(CameraId);
    ASSERT(Camera);
    const hz::RenderCamera& CameraState = *Camera;
    const float             AspectInverse = (float)Context.getHeight() / (float)Context.getWidth();
    const Matrix4           Current = CameraViewProjection(CameraState.current, CameraState.lens, AspectInverse);
    const FrameData         FrameValues = { .viewProjection = Current,
                                            .previousViewProjection =
                                                CameraViewProjection(CameraState.previous, CameraState.previousLens, AspectInverse),
                                            .inverseViewProjection = inverse(Current),
                                            .eye = CameraState.current.getCol3() };
    const hz::GPUBuffer&    Frame = ConstantBuffer.Buffer;
    const bool              Updated = Context.updateBuffer(Frame, 0, &FrameValues, sizeof(FrameValues));
    ASSERT(Updated);
    hz::CommandList& Commands = Context.acquireCommandList();
    GpuSceneData.upload(Commands);
    Draws.Prepare(Commands, Frame, View);
    // Frame shape the passes depend on; the shadow pass publishes its intensity once it ran.
    Shadows->Data.environmentMipCount = Environment->GetPrefilteredMipCount();
    Shadows->Data.rtShadowEnabled = RayTracing->IsSupported() && RayTracing->Settings.Shadows ? 1u : 0u;
    Shadows->Data.rtAOEnabled = RayTracing->IsSupported() && RayTracing->Settings.AO ? 1u : 0u;
    const hz::GPUTexture& Backbuffer = Context.getCurrentBackbuffer();
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
    ConstantBuffer.Submission = Context.submit(Commands, FrameState.Backbuffer);
    GpuSceneData.endFrame(ConstantBuffer.Submission);
}
