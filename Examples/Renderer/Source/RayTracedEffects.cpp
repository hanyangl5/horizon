#include "RayTracedEffects.h"
#include "Profiler/IProfiler.h"
#include "RayTracingScene.h"

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
// Mirrors the RootConstant0 block of the denoise shaders field for field.
struct RTDenoiseConstants
{
    uint32_t FrameIndex;
    uint32_t InputIndex;
    uint32_t HistoryIndex;
    uint32_t DepthIndex;
    uint32_t PreviousDepthIndex;
    uint32_t NormalIndex;
    uint32_t OutputIndex;
    uint32_t HistoryValid;
    float    TemporalAlpha;
    float    DepthThreshold;
    float    PlaneSigma;
    uint32_t StepWidth;
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
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "RTShadowAO.slang" },
        .Name = "Renderer.RT.ShadowAO",
    });
    TemporalPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "rt_denoise_temporal.comp.slang" },
        .Name = "Renderer.RT.Temporal",
    });
    AtrousPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_atrous.comp.slang" },
        .Name = "Renderer.RT.Atrous",
    });
    DepthCopyPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSCopyDepth" } }, .pFileName = "RTDenoise.slang" },
        .Name = "Renderer.RT.DepthHistory",
    });
    ReflectionTemporalPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "rt_reflection_temporal.comp.slang" },
        .Name = "Renderer.RT.ReflectionTemporal",
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
    ASSERT(MaterialSampler.IsValid() && ShadowPipeline.IsValid() && TemporalPipeline.IsValid() && AtrousPipeline.IsValid() &&
           DepthCopyPipeline.IsValid() && ReflectionPipeline.IsValid() && ReflectionTemporalPipeline.IsValid());
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
           GBuffer.Depth && Lighting.LightingFrame);
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
                                .ShadowsEnabled = Settings.Shadows,
                                .AoEnabled = Settings.AO,
                                .AoSamples = TF_MAX(Settings.AOSamples, 1u),
                                .FrameNumber = FrameIndex++,
                                .AlphaTestMethod = Settings.ShadowAlphaTestMethod,
                                .RetraceMaxDepth = Settings.RetraceMaxDepth,
                                .AoRadius = Settings.AORadius,
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

    Commands.BeginGpuTimestamp("RT Denoise");
    const hz::GPUTexture& History = *Targets.History;
    const hz::GPUTexture& PreviousHistory = *Targets.PreviousHistory;
    const hz::GPUTexture& PreviousFrameDepth = *Targets.PreviousFrameDepth;
    RTDenoiseConstants    Denoise = {
           .FrameIndex = Frame.GetSrvIndex(),
           .InputIndex = Result.GetSrvIndex(),
           .HistoryIndex = PreviousHistory.GetSrvIndex(),
           .DepthIndex = GBuffer.Depth->GetSrvIndex(),
           .PreviousDepthIndex = PreviousFrameDepth.GetSrvIndex(),
           .NormalIndex = GBuffer.NormalMaterial->GetSrvIndex(),
           .OutputIndex = History.GetUavIndex(),
           .HistoryValid = Targets.HistoryValid && !MarkedDirty,
           .TemporalAlpha = Settings.TemporalAlpha,
           .DepthThreshold = Settings.TemporalDepthThreshold,
           .PlaneSigma = Settings.AtrousPlaneSigma,
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
        Denoise.InputIndex = Passes[Pass]->GetSrvIndex();
        Denoise.OutputIndex = Passes[Pass + 1]->GetUavIndex();
        Denoise.StepWidth = 1u << Pass;
        Commands.SetPipeline(AtrousPipeline);
        Commands.SetRootConstants(Denoise);
        Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                          { .sampledTextures = { Passes[Pass], GBuffer.Depth, GBuffer.NormalMaterial },
                            .storageTextures = { Passes[Pass + 1] },
                            .buffers = { &Frame } });
    }
    Denoise.OutputIndex = Targets.PreviousDepth->GetUavIndex();
    Commands.SetPipeline(DepthCopyPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { GBuffer.Depth }, .storageTextures = { Targets.PreviousDepth }, .buffers = { &Frame } });
    Commands.EndGpuTimestamp();

    Commands.BeginGpuTimestamp("RT Reflection");
    Constants.OutputIndex = Reflection.GetUavIndex();
    if (!Settings.Reflection)
    {
        Constants.TlasIndex = UINT32_MAX;
    }
    Commands.SetPipeline(ReflectionPipeline);
    Commands.SetRootConstants(Constants);
    Commands.TraceRays(ReflectionPipeline, Width, Height, 1,
                       { .sampledTextures = { GBuffer.NormalMaterial, GBuffer.Depth, &BlueNoise, Lighting.Environment, Lighting.Irradiance,
                                              Lighting.BRDFLut },
                         .storageTextures = { &Reflection },
                         .buffers = { Lighting.LightingFrame },

                         .pShared = &SceneDependencies });

    // Reads last frame's view depth, which the depth copy above did not overwrite: it writes this frame's copy.
    const hz::GPUTexture& ReflectionHistory = *Targets.ReflectionHistory;
    const hz::GPUTexture& PreviousReflectionHistory = *Targets.PreviousReflectionHistory;
    Denoise.InputIndex = Reflection.GetSrvIndex();
    Denoise.HistoryIndex = PreviousReflectionHistory.GetSrvIndex();
    Denoise.OutputIndex = ReflectionHistory.GetUavIndex();
    Denoise.HistoryValid = Targets.ReflectionHistoryValid && Targets.HistoryValid && !MarkedDirty;
    Denoise.TemporalAlpha = Settings.ReflectionTemporalAlpha;
    Commands.SetPipeline(ReflectionTemporalPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &Reflection, &PreviousReflectionHistory, GBuffer.Depth, &PreviousFrameDepth },
                        .storageTextures = { &ReflectionHistory },
                        .buffers = { &Frame } });
    MarkedDirty = false;
    Commands.EndGpuTimestamp();
}
