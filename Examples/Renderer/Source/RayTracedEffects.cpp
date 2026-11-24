#include "RayTracedEffects.h"
#include "Profiler/IProfiler.h"
#include "RayTracingScene.h"
#include "ScreenSpaceDenoise.h"

// Mirrors the RootConstant0 block of RTShadowAO.slang field for field.
struct RTConstants
{
    uint32_t FrameIndexBuffer;
    uint32_t LightingIndex;
    uint32_t DrawsIndex;
    uint32_t MaterialsIndex;
    uint32_t TexturesIndex;
    uint32_t SamplerIndex;
    uint32_t TlasIndex;
    uint32_t NormalIndex;
    uint32_t DepthIndex;
    uint32_t OutputIndex;
    uint32_t BlueNoiseIndex;
    uint32_t EnvironmentIndex;
    uint32_t IrradianceIndex;
    uint32_t BrdfIndex;
    uint32_t EnvironmentSamplerIndex;
    uint32_t ShadowsEnabled;
    uint32_t AoEnabled;
    uint32_t AoSamples;
    uint32_t FrameNumber;
    uint32_t AlphaTestMethod;
    uint32_t RetraceMaxDepth;
    float    AoRadius;
    float    ShadowRadius;
};
static_assert(sizeof(RTConstants) == 23 * sizeof(uint32_t));
hz::CVar GRTShadows("r.RayTracing.Shadows", true, "Ray-traced shadows.");
hz::CVar GRTAO("r.RayTracing.AO", true, "Ray-traced AO takes precedence over GTAO.");
hz::CVar GRTReflections("r.RayTracing.Reflections", true, "Ray-traced reflections.");
static hz::CVar GRTAORadius("r.RayTracing.AO.Radius", 2.0f, 0.0001f, "Maximum occlusion ray distance in meters.");

RayTracedEffects::RayTracedEffects(hz::RenderContext& Context): Context(Context), Supported(Context.SupportsRayTracing())
{
    GRTShadows.SetAvailable(Supported);
    GRTAO.SetAvailable(Supported);
    GRTReflections.SetAvailable(Supported);
    GRTAORadius.SetAvailable(Supported);
    if (!Supported)
    {
        return;
    }
    RayScene = hz::MakeUnique<RayTracingScene>(Context);
    MaterialSampler = Context.CreateSampler();
    ShadowPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "RTShadowAO.slang" },
        .Name = "Renderer.RT.ShadowAO",
    });
    Denoiser = hz::MakeUnique<ScreenSpaceDenoise>(Context);
    ReflectionPipeline = Context.CreateRayTracingPipeline({
        .pFileName = "RTReflection.slang",
        .pRayGeneration = "rayGenMain",
        .missShaders = { "missMain" },
        .hitGroups = { { .Name = "ReflectionHit", .pClosestHit = "closestHitMain", .pAnyHit = "anyHitFoliage" } },
        .payloadSize = 16,
        .maxRecursionDepth = 2,
        .Name = "Renderer.RT.Reflection",
    });
    ASSERT(MaterialSampler.IsValid() && ShadowPipeline.IsValid() && ReflectionPipeline.IsValid());
}

RayTracedEffects::~RayTracedEffects() = default;

bool RayTracedEffects::Load()
{
    if (!Supported)
        return true;
    if (!BlueNoise.IsValid())
    {
        BlueNoise = Context.LoadTexture(DirectoryShaderSources, "../Assets/blue_noise_128x128x64.dds");
    }
    MarkDirty();
    if (!BlueNoise.IsValid() || BlueNoise.GetWidth() != 128 || BlueNoise.GetHeight() != 8192)
    {
        LOGF(Error, "Ray traced effects require Examples/Renderer/Assets/blue_noise_128x128x64.dds");
        return false;
    }
    return true;
}

void RayTracedEffects::MarkDirty() { MarkedDirty = true; }

void RayTracedEffects::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                               const hz::SceneView& View, const hz::GpuDrawList& Draws, const LightingResources& Lighting,
                               const RayTracedEffectsTargets& Targets)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RayTracedEffects::execute");
    if (!Supported)
    {
        return;
    }
    ASSERT(Targets.Result && Targets.Scratch && Targets.Reflection && Targets.History && Targets.PreviousHistory && Targets.PreviousDepth &&
           Targets.PreviousFrameDepth && Targets.ReflectionHistory && Targets.PreviousReflectionHistory && GBuffer.NormalMaterial &&
           GBuffer.Depth && GBuffer.Motion && Lighting.LightingFrame);
    const hz::GPUTexture& Result = *Targets.Result;
    const hz::GPUTexture& Reflection = *Targets.Reflection;
    const uint32_t        Width = Result.GetWidth();
    const uint32_t        Height = Result.GetHeight();
    const HistorySettings Current = { .Parameters = Settings, .Shadows = GRTShadows.GetBool(), .AO = GRTAO.GetBool(),
                                      .Reflections = GRTReflections.GetBool(), .AORadius = GRTAORadius.GetFloat() };
    if (!(Current == PreviousSettings))
    {
        MarkDirty();
    }
    PreviousSettings = Current;
    if (AssetRevision != View.GpuScene.GetAssetTables().revision)
    {
        MarkDirty();
    }
    AssetRevision = View.GpuScene.GetAssetTables().revision;
    Commands.BeginGpuTimestamp("RT Acceleration Structures");
    RayScene->Update(Commands, View, Draws);
    Commands.EndGpuTimestamp();
    const hz::GPUAccelerationStructure& TLAS = RayScene->GetSceneTLAS();
    const hz::GPUBuffer&                DrawData = Draws.GetDrawData(View.FrameIndex);
    auto        Index = [](const hz::GPUTexture* Texture) { return Texture && Texture->IsValid() ? Texture->GetSrvIndex() : UINT32_MAX; };
    RTConstants                         Constants = {
                                .FrameIndexBuffer = Frame.GetSrvIndex(),
                                .LightingIndex = Lighting.LightingFrame->GetSrvIndex(),
                                .DrawsIndex = DrawData.GetSrvIndex(),
                                .MaterialsIndex = View.GpuScene.GetBuffers().materials.GetSrvIndex(),
                                .TexturesIndex = View.GpuScene.GetBuffers().textures.GetSrvIndex(),
                                .SamplerIndex = MaterialSampler.GetIndex(),
                                .TlasIndex = TLAS.IsValid() ? TLAS.GetSrvIndex() : UINT32_MAX,
                                .NormalIndex = GBuffer.NormalMaterial->GetSrvIndex(),
                                .DepthIndex = GBuffer.Depth->GetSrvIndex(),
                                .OutputIndex = Result.GetUavIndex(),
                                .BlueNoiseIndex = BlueNoise.GetSrvIndex(),
                                .EnvironmentIndex = Index(Lighting.Environment),
                                .IrradianceIndex = Index(Lighting.Irradiance),
                                .BrdfIndex = Index(Lighting.BRDFLut),
                                .EnvironmentSamplerIndex = Lighting.EnvironmentSampler ? Lighting.EnvironmentSampler->GetIndex() : UINT32_MAX,
                                .ShadowsEnabled = Current.Shadows,
                                .AoEnabled = Current.AO,
                                .AoSamples = TF_MAX(Settings.AOSamples, 1u),
                                .FrameNumber = FrameIndex++,
                                .AlphaTestMethod = Settings.ShadowAlphaTestMethod,
                                .RetraceMaxDepth = Settings.RetraceMaxDepth,
                                .AoRadius = Current.AORadius,
                                .ShadowRadius = Settings.ShadowRadius,
    };
    const hz::Dependencies SceneDependencies = Draws.GetDependencies(Frame, View);
    Commands.BeginGpuTimestamp("RT Shadow AO");
    Commands.SetPipeline(ShadowPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { GBuffer.NormalMaterial, GBuffer.Depth, &BlueNoise },
                        .storageTextures = { &Result },
                        .buffers = { Lighting.LightingFrame },

                        .pShared = &SceneDependencies });
    Commands.EndGpuTimestamp();

    Denoiser->DenoiseShadowAO(Commands, Frame, GBuffer, Settings, Targets, !MarkedDirty);
    Denoiser->StoreDepthHistory(Commands, Frame, *GBuffer.Depth, *Targets.PreviousDepth);

    Commands.BeginGpuTimestamp("RT Reflection");
    Constants.OutputIndex = Reflection.GetUavIndex();
    if (!Current.Reflections)
    {
        Constants.TlasIndex = UINT32_MAX;
    }
    Commands.SetPipeline(ReflectionPipeline);
    Commands.SetRootConstants(Constants);
    const hz::GPUBuffer* LightingBuffers[] = { Lighting.LightingFrame, Lighting.Lights, Lighting.ActiveLights };
    Commands.TraceRays(ReflectionPipeline, Width, Height, 1,
                       { .sampledTextures = { GBuffer.NormalMaterial, GBuffer.Depth, &BlueNoise, Lighting.Environment, Lighting.Irradiance,
                                              Lighting.BRDFLut },
                         .storageTextures = { &Reflection },
                         .buffers = { LightingBuffers, Lighting.Lights ? 3u : 1u },
                         .pShared = &SceneDependencies });

    Commands.EndGpuTimestamp();
    Denoiser->DenoiseReflections(Commands, Frame, GBuffer, Settings, Targets, !MarkedDirty);
    MarkedDirty = false;
}
