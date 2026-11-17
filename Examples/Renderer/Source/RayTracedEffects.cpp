#include "RayTracedEffects.h"
#include "Profiler/IProfiler.h"
#include "RayTracingScene.h"

// Mirrors the RootConstant0 block of RTShadowAO.slang field for field.
struct RTConstants
{
    uint32_t frame;
    uint32_t lighting;
    uint32_t draws;
    uint32_t materials;
    uint32_t textures;
    uint32_t sampler;
    uint32_t tlas;
    uint32_t normal;
    uint32_t Depth;
    uint32_t output;
    uint32_t blueNoise;
    uint32_t environment;
    uint32_t irradiance;
    uint32_t brdf;
    uint32_t environmentSampler;
    uint32_t shadows;
    uint32_t ao;
    uint32_t aoSamples;
    uint32_t frameNumber;
    uint32_t alphaMethod;
    uint32_t retraceDepth;
    float    aoRadius;
    float    shadowRadius;
};
static_assert(sizeof(RTConstants) == 23 * sizeof(uint32_t));
// Mirrors the RootConstant0 block of the denoise shaders field for field.
struct RTDenoiseConstants
{
    uint32_t frame;
    uint32_t input;
    uint32_t history;
    uint32_t Depth;
    uint32_t previousDepth;
    uint32_t normal;
    uint32_t output;
    uint32_t historyValid;
    float    alpha;
    float    depthThreshold;
    float    planeSigma;
    uint32_t step;
};
static_assert(sizeof(RTDenoiseConstants) == 12 * sizeof(uint32_t));

RayTracedEffects::RayTracedEffects(hz::RenderContext& Context): Context(Context), Supported(Context.SupportsRayTracing())
{
    if (!Supported)
    {
        return;
    }
    RayScene = hz::MakeUnique<RayTracingScene>(Context);
    MaterialSampler = Context.CreateSampler();
    ShadowPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "main" } }, .pFileName = "RTShadowAO.slang" },
        .Name = "Renderer.RT.ShadowAO",
    });
    TemporalPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "main" } },
                        .pFileName = "rt_denoise_temporal.comp.slang" },
        .Name = "Renderer.RT.Temporal",
    });
    AtrousPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_atrous.comp.slang" },
        .Name = "Renderer.RT.Atrous",
    });
    DepthCopyPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSCopyDepth" } }, .pFileName = "RTDenoise.slang" },
        .Name = "Renderer.RT.DepthHistory",
    });
    ReflectionPipeline = Context.CreateRayTracingPipeline({
        .pFileName = "RTReflection.slang",
        .pRayGeneration = "rayGenMain",
        .missShaders = { "missMain" },
        .hitGroups = { { .Name = "ReflectionHit", .pClosestHit = "closestHitMain", .pAnyHit = "anyHitFoliage" } },
        .payloadSize = 16,
        .maxRecursionDepth = 2,
        .Name = "Renderer.RT.Reflection",
    });
    ASSERT(MaterialSampler.IsValid() && ShadowPipeline.IsValid() && TemporalPipeline.IsValid() &&
           AtrousPipeline.IsValid() && DepthCopyPipeline.IsValid() && ReflectionPipeline.IsValid());
}

RayTracedEffects::~RayTracedEffects() = default;

bool RayTracedEffects::Load()
{
    if (!Supported)
        return true;
    if (!BlueNoise.IsValid())
        BlueNoise = Context.LoadTexture(DirectoryShaderSources, "../Assets/blue_noise_128x128x64.dds");
    MarkDirty();
    if (!BlueNoise.IsValid() || BlueNoise.GetWidth() != 128 || BlueNoise.GetHeight() != 8192)
    {
        LOGF(Error, "Ray traced effects require Examples/Renderer/Assets/blue_noise_128x128x64.dds");
        return false;
    }
    return true;
}

void RayTracedEffects::MarkDirty() { MarkedDirty = true; }

void RayTracedEffects::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const hz::SceneView& View,
                               const hz::GpuDrawList& Draws, const LightingResources& Lighting, const RayTracedEffectsTargets& Targets)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RayTracedEffects::execute");
    if (!Supported)
    {
        return;
    }
    ASSERT(Targets.Result && Targets.Scratch && Targets.Reflection && Targets.History && Targets.PreviousHistory &&
           Targets.PreviousDepth && Targets.PreviousFrameDepth && GBuffer.NormalMaterial && GBuffer.Depth && Lighting.LightingFrame);
    const hz::GPUTexture& Result = *Targets.Result;
    const hz::GPUTexture& Scratch = *Targets.Scratch;
    const hz::GPUTexture& Reflection = *Targets.Reflection;
    const uint32_t        Width = Result.GetWidth();
    const uint32_t        Height = Result.GetHeight();
    if (!(Settings == PreviousSettings))
    {
        MarkDirty();
    }
    PreviousSettings = Settings;
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
    RTConstants Constants = {
                                .frame = Frame.GetSrvIndex(),
                                .lighting = Lighting.LightingFrame->GetSrvIndex(),
                                .draws = DrawData.GetSrvIndex(),
                                .materials = View.GpuScene.GetBuffers().materials.GetSrvIndex(),
                                .textures = View.GpuScene.GetBuffers().textures.GetSrvIndex(),
                                .sampler = MaterialSampler.GetIndex(),
                                .tlas = TLAS.IsValid() ? TLAS.GetSrvIndex() : UINT32_MAX,
                                .normal = GBuffer.NormalMaterial->GetSrvIndex(),
                                .Depth = GBuffer.Depth->GetSrvIndex(),
                                .output = Result.GetUavIndex(),
                                .blueNoise = BlueNoise.GetSrvIndex(),
                                .environment = Index(Lighting.Environment),
                                .irradiance = Index(Lighting.Irradiance),
                                .brdf = Index(Lighting.BRDFLut),
                                .environmentSampler = Lighting.EnvironmentSampler ? Lighting.EnvironmentSampler->GetIndex() : UINT32_MAX,
                                .shadows = Settings.Shadows,
                                .ao = Settings.AO,
                                .aoSamples = TF_MAX(Settings.AOSamples, 1u),
                                .frameNumber = FrameIndex++,
                                .alphaMethod = Settings.ShadowAlphaTestMethod,
                                .retraceDepth = Settings.RetraceMaxDepth,
                                .aoRadius = Settings.AORadius,
                                .shadowRadius = Settings.ShadowRadius,
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

    Commands.BeginGpuTimestamp("RT Denoise");
    const hz::GPUTexture& History = *Targets.History;
    const hz::GPUTexture& PreviousHistory = *Targets.PreviousHistory;
    const hz::GPUTexture& PreviousFrameDepth = *Targets.PreviousFrameDepth;
    RTDenoiseConstants    Denoise = {
        .frame = Frame.GetSrvIndex(),
        .input = Result.GetSrvIndex(),
        .history = PreviousHistory.GetSrvIndex(),
        .Depth = GBuffer.Depth->GetSrvIndex(),
        .previousDepth = PreviousFrameDepth.GetSrvIndex(),
        .normal = GBuffer.NormalMaterial->GetSrvIndex(),
        .output = History.GetUavIndex(),
        .historyValid = Targets.HistoryValid && !MarkedDirty,
        .alpha = Settings.TemporalAlpha,
        .depthThreshold = Settings.TemporalDepthThreshold,
        .planeSigma = Settings.AtrousPlaneSigma,
    };
    Commands.SetPipeline(TemporalPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &Result, &PreviousHistory, GBuffer.Depth, &PreviousFrameDepth },
                        .storageTextures = { &History },
                        .buffers = { &Frame } });
    // The traced input is consumed, so the a-trous iterations ping-pong through Scratch and finish in Result.
    const hz::GPUTexture* Passes[] = { &History, &Scratch, &Result };
    for (uint32_t Pass = 0; Pass < 2; ++Pass)
    {
        Denoise.input = Passes[Pass]->GetSrvIndex();
        Denoise.output = Passes[Pass + 1]->GetUavIndex();
        Denoise.step = 1u << Pass;
        Commands.SetPipeline(AtrousPipeline);
        Commands.SetRootConstants(Denoise);
        Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                          { .sampledTextures = { Passes[Pass], GBuffer.Depth, GBuffer.NormalMaterial },
                            .storageTextures = { Passes[Pass + 1] },
                            .buffers = { &Frame } });
    }
    Denoise.output = Targets.PreviousDepth->GetUavIndex();
    Commands.SetPipeline(DepthCopyPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { GBuffer.Depth }, .storageTextures = { Targets.PreviousDepth }, .buffers = { &Frame } });
    MarkedDirty = false;
    Commands.EndGpuTimestamp();

    Commands.BeginGpuTimestamp("RT Reflection");
    Constants.output = Reflection.GetUavIndex();
    if (!Settings.Reflection)
    {
        Constants.tlas = UINT32_MAX;
    }
    Commands.SetPipeline(ReflectionPipeline);
    Commands.SetRootConstants(Constants);
    Commands.TraceRays(ReflectionPipeline, Width, Height, 1,
                       { .sampledTextures = { GBuffer.NormalMaterial, GBuffer.Depth, &BlueNoise, Lighting.Environment,
                                              Lighting.Irradiance, Lighting.BRDFLut },
                         .storageTextures = { &Reflection },
                         .buffers = { Lighting.LightingFrame },

                         .pShared = &SceneDependencies });
    Commands.EndGpuTimestamp();
}
