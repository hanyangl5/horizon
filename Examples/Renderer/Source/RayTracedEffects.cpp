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

void RayTracedEffects::Resize(uint32_t Width, uint32_t Height)
{
    if (!Supported)
    {
        return;
    }
    ASSERT(Width && Height);
    this->Width = Width;
    this->Height = Height;
    const DescriptorType Views = DescriptorType(DescriptorTypeTexture | DescriptorTypeRwTexture);
    hz::TextureDesc      Desc = { .Width = Width,
                                  .Height = Height,
                                  .Format = hz::Format::R8G8_UNORM,
                                  .StartState = ResourceStateUnorderedAccess,
                                  .Descriptors = Views,
                                  .Flags = TextureCreationFlagForce2D,
                                  .Name = "Renderer.RT.ShadowAO" };
    Result = Context.CreateTexture(Desc);
    Desc.Format = hz::Format::R16G16_SFLOAT;
    Desc.Name = "Renderer.RT.DenoiseScratch";
    Scratch = Context.CreateTexture(Desc);
    // Shadow, AO and the per-pixel history length.
    Desc.Format = hz::Format::R16G16B16A16_SFLOAT;
    Desc.Name = "Renderer.RT.History";
    for (hz::GPUTexture& Texture : History)
    {
        Texture = Context.CreateTexture(Desc);
    }
    Desc.Format = hz::Format::R32_SFLOAT;
    Desc.Name = "Renderer.RT.PreviousDepth";
    PreviousDepth = Context.CreateTexture(Desc);
    Desc.Format = hz::Format::R32G32B32A32_SFLOAT;
    Desc.Name = "Renderer.RT.Reflection";
    Reflection = Context.CreateTexture(Desc);
    if (!BlueNoise.IsValid())
    {
        BlueNoise = Context.LoadTexture(DirectoryShaderSources, "../Assets/blue_noise_128x128x64.dds");
    }
    HistoryIndex = 0;
    MarkDirty();
    ASSERT(Result.IsValid() && Scratch.IsValid() && History[0].IsValid() && History[1].IsValid() && PreviousDepth.IsValid() &&
           Reflection.IsValid() && BlueNoise.IsValid() && BlueNoise.GetWidth() == 128 && BlueNoise.GetHeight() == 8192);
}

void RayTracedEffects::MarkDirty() { MarkedDirty = true; }

void RayTracedEffects::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const SceneView& View,
                               const GpuDrawList& Draws, const LightingResources& Lighting)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RayTracedEffects::execute");
    if (!Supported)
    {
        return;
    }
    ASSERT(Width && Height && GBuffer.NormalMaterial && GBuffer.Depth && Lighting.LightingFrame);
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
    const uint32_t     Current = HistoryIndex;
    const uint32_t     Previous = 1 - Current;
    RTDenoiseConstants Denoise = {
        .frame = Frame.GetSrvIndex(),
        .input = Result.GetSrvIndex(),
        .history = History[Previous].GetSrvIndex(),
        .Depth = GBuffer.Depth->GetSrvIndex(),
        .previousDepth = PreviousDepth.GetSrvIndex(),
        .normal = GBuffer.NormalMaterial->GetSrvIndex(),
        .output = History[Current].GetUavIndex(),
        .historyValid = !MarkedDirty,
        .alpha = Settings.TemporalAlpha,
        .depthThreshold = Settings.TemporalDepthThreshold,
        .planeSigma = Settings.AtrousPlaneSigma,
    };
    Commands.SetPipeline(TemporalPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &Result, &History[Previous], GBuffer.Depth, &PreviousDepth },
                        .storageTextures = { &History[Current] },
                        .buffers = { &Frame } });
    // The traced input is consumed, so the a-trous iterations ping-pong through Scratch and finish in Result.
    const hz::GPUTexture* Passes[] = { &History[Current], &Scratch, &Result };
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
    Denoise.output = PreviousDepth.GetUavIndex();
    Commands.SetPipeline(DepthCopyPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { GBuffer.Depth }, .storageTextures = { &PreviousDepth }, .buffers = { &Frame } });
    HistoryIndex = Previous;
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
