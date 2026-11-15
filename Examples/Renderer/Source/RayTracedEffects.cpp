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
    uint32_t depth;
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
    float    aoPower;
    float    shadowRadius;
};
// Mirrors the RootConstant0 block of the denoise shaders field for field.
struct RTDenoiseConstants
{
    uint32_t frame;
    uint32_t input;
    uint32_t history;
    uint32_t depth;
    uint32_t previousDepth;
    uint32_t output;
    uint32_t sampler;
    uint32_t historyValid;
    float    alpha;
    float    depthThreshold;
    uint32_t horizontal;
    uint32_t step;
};

RayTracedEffects::RayTracedEffects(hz::RenderContext& Context): Context(Context), Supported(Context.supportsRayTracing())
{
    if (!Supported)
    {
        return;
    }
    RayScene = hz::make_unique<RayTracingScene>(Context);
    Sampler = Context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    MaterialSampler = Context.createSampler();
    ShadowPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "RTShadowAO.slang" },
        .pName = "Renderer.RT.ShadowAO",
    });
    TemporalPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "rt_denoise_temporal.comp.slang" },
        .pName = "Renderer.RT.Temporal",
    });
    SpatialPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_spatial.comp.slang" },
        .pName = "Renderer.RT.Spatial",
    });
    AtrousPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_atrous.comp.slang" },
        .pName = "Renderer.RT.Atrous",
    });
    DepthCopyPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSCopyDepth" } }, .pFileName = "RTDenoise.slang" },
        .pName = "Renderer.RT.DepthHistory",
    });
    ReflectionPipeline = Context.createRayTracingPipeline({
        .pFileName = "RTReflection.slang",
        .pRayGeneration = "rayGenMain",
        .missShaders = { "missMain" },
        .hitGroups = { { .pName = "ReflectionHit", .pClosestHit = "closestHitMain", .pAnyHit = "anyHitFoliage" } },
        .payloadSize = 16,
        .maxRecursionDepth = 2,
        .pName = "Renderer.RT.Reflection",
    });
    ASSERT(Sampler.isValid() && MaterialSampler.isValid() && ShadowPipeline.isValid() && TemporalPipeline.isValid() &&
           SpatialPipeline.isValid() && AtrousPipeline.isValid() && DepthCopyPipeline.isValid() && ReflectionPipeline.isValid());
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
    const DescriptorType Views = DescriptorType(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    hz::TextureDesc      Desc = { .width = Width,
                                  .height = Height,
                                  .format = hz::Format::R8G8_UNORM,
                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                  .descriptors = Views,
                                  .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                  .pName = "Renderer.RT.ShadowAO" };
    Result = Context.createTexture(Desc);
    Desc.pName = "Renderer.RT.DenoiseScratch";
    Scratch = Context.createTexture(Desc);
    Desc.pName = "Renderer.RT.History";
    for (hz::GPUTexture& Texture : History)
    {
        Texture = Context.createTexture(Desc);
    }
    Desc.format = hz::Format::R32_SFLOAT;
    Desc.pName = "Renderer.RT.PreviousDepth";
    PreviousDepth = Context.createTexture(Desc);
    Desc.format = hz::Format::R32G32B32A32_SFLOAT;
    Desc.pName = "Renderer.RT.Reflection";
    Reflection = Context.createTexture(Desc);
    if (!BlueNoise.isValid())
    {
        BlueNoise = Context.loadTexture(RD_SHADER_SOURCES, "../Assets/blue_noise_128x128x64.dds");
    }
    HistoryIndex = 0;
    MarkDirty();
    ASSERT(Result.isValid() && Scratch.isValid() && History[0].isValid() && History[1].isValid() && PreviousDepth.isValid() &&
           Reflection.isValid() && BlueNoise.isValid() && BlueNoise.getWidth() == 128 && BlueNoise.getHeight() == 8192);
}

void RayTracedEffects::MarkDirty() { MarkedDirty = true; }

void RayTracedEffects::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const SceneView& View,
                               const GpuDrawList& Draws, const LightingResources& Lighting)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RayTracedEffects::Execute");
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
    if (AssetRevision != View.gpuScene.getAssetTables().revision)
    {
        MarkDirty();
    }
    AssetRevision = View.gpuScene.getAssetTables().revision;
    Commands.beginGpuTimestamp("RT Acceleration Structures");
    RayScene->Update(Commands, View, Draws);
    Commands.endGpuTimestamp();
    const hz::GPUAccelerationStructure& TLAS = RayScene->GetSceneTLAS();
    const hz::GPUBuffer&                DrawData = Draws.GetDrawData(View.frameIndex);
    auto        Index = [](const hz::GPUTexture* Texture) { return Texture && Texture->isValid() ? Texture->getSrvIndex() : UINT32_MAX; };
    RTConstants Constants = {
                                .frame = Frame.getSrvIndex(),
                                .lighting = Lighting.LightingFrame->getSrvIndex(),
                                .draws = DrawData.getSrvIndex(),
                                .materials = View.gpuScene.getBuffers().materials.getSrvIndex(),
                                .textures = View.gpuScene.getBuffers().textures.getSrvIndex(),
                                .sampler = MaterialSampler.getIndex(),
                                .tlas = TLAS.isValid() ? TLAS.getSrvIndex() : UINT32_MAX,
                                .normal = GBuffer.NormalMaterial->getSrvIndex(),
                                .depth = GBuffer.Depth->getSrvIndex(),
                                .output = Result.getUavIndex(),
                                .blueNoise = BlueNoise.getSrvIndex(),
                                .environment = Index(Lighting.Environment),
                                .irradiance = Index(Lighting.Irradiance),
                                .brdf = Index(Lighting.BRDFLut),
                                .environmentSampler = Lighting.EnvironmentSampler ? Lighting.EnvironmentSampler->getIndex() : UINT32_MAX,
                                .shadows = Settings.Shadows,
                                .ao = Settings.AO,
                                .aoSamples = TF_MAX(Settings.AOSamples, 1u),
                                .frameNumber = FrameIndex++,
                                .alphaMethod = Settings.ShadowAlphaTestMethod,
                                .retraceDepth = Settings.RetraceMaxDepth,
                                .aoRadius = Settings.AORadius,
                                .aoPower = Settings.AOPower,
                                .shadowRadius = Settings.ShadowRadius,
    };
    const hz::Dependencies SceneDependencies = Draws.GetDependencies(Frame, View);
    Commands.beginGpuTimestamp("RT Shadow AO");
    Commands.setPipeline(ShadowPipeline);
    Commands.setRootConstants(Constants);
    Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { GBuffer.NormalMaterial, GBuffer.Depth, &BlueNoise },
                        .storageTextures = { &Result },
                        .buffers = { Lighting.LightingFrame },

                        .pShared = &SceneDependencies });
    Commands.endGpuTimestamp();

    Commands.beginGpuTimestamp("RT Denoise");
    const uint32_t     Current = HistoryIndex;
    const uint32_t     Previous = 1 - Current;
    RTDenoiseConstants Denoise = {
        .frame = Frame.getSrvIndex(),
        .input = Result.getSrvIndex(),
        .history = History[Previous].getSrvIndex(),
        .depth = GBuffer.Depth->getSrvIndex(),
        .previousDepth = PreviousDepth.getSrvIndex(),
        .output = History[Current].getUavIndex(),
        .sampler = Sampler.getIndex(),
        .historyValid = !MarkedDirty,
        .alpha = Settings.TemporalAlpha,
        .depthThreshold = Settings.TemporalDepthThreshold,
    };
    Commands.setPipeline(TemporalPipeline);
    Commands.setRootConstants(Denoise);
    Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &Result, &History[Previous], GBuffer.Depth, &PreviousDepth },
                        .storageTextures = { &History[Current] },
                        .buffers = { &Frame } });
    const bool Atrous = Settings.DenoiseMode == RTDenoiseMode::Atrous;
    Denoise.depthThreshold = Atrous ? Settings.AtrousDepthSigma : Settings.SpatialDepthThreshold;
    for (uint32_t Pass = 0; Pass < 2; ++Pass)
    {
        const hz::GPUTexture& Input = Pass ? Scratch : History[Current];
        const hz::GPUTexture& Output = Pass ? Result : Scratch;
        Denoise.input = Input.getSrvIndex();
        Denoise.output = Output.getUavIndex();
        Denoise.horizontal = Pass == 0;
        Denoise.step = Pass + 1;
        Commands.setPipeline(Atrous ? AtrousPipeline : SpatialPipeline);
        Commands.setRootConstants(Denoise);
        Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                          { .sampledTextures = { &Input, GBuffer.Depth }, .storageTextures = { &Output } });
    }
    Denoise.output = PreviousDepth.getUavIndex();
    Commands.setPipeline(DepthCopyPipeline);
    Commands.setRootConstants(Denoise);
    Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { GBuffer.Depth }, .storageTextures = { &PreviousDepth } });
    HistoryIndex = Previous;
    MarkedDirty = false;
    Commands.endGpuTimestamp();

    Commands.beginGpuTimestamp("RT Reflection");
    Constants.output = Reflection.getUavIndex();
    if (!Settings.Reflection)
    {
        Constants.tlas = UINT32_MAX;
    }
    Commands.setPipeline(ReflectionPipeline);
    Commands.setRootConstants(Constants);
    Commands.traceRays(ReflectionPipeline, Width, Height, 1,
                       { .sampledTextures = { GBuffer.NormalMaterial, GBuffer.Depth, &BlueNoise, Lighting.Environment,
                                              Lighting.Irradiance, Lighting.BRDFLut },
                         .storageTextures = { &Reflection },
                         .buffers = { Lighting.LightingFrame },

                         .pShared = &SceneDependencies });
    Commands.endGpuTimestamp();
}
