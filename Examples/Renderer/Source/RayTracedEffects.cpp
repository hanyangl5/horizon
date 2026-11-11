#include "RayTracedEffects.h"
#include "Profiler/IProfiler.h"
#include "RayTracingScene.h"

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

RayTracedEffects::RayTracedEffects(hz::RenderContext& context): context(context), supported(context.supportsRayTracing())
{
    if (!supported)
    {
        return;
    }
    rayScene = hz::make_unique<RayTracingScene>(context);
    sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    materialSampler = context.createSampler();
    shadowPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "RTShadowAO.slang" },
        .pName = "Renderer.RT.ShadowAO",
    });
    temporalPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "rt_denoise_temporal.comp.slang" },
        .pName = "Renderer.RT.Temporal",
    });
    spatialPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_spatial.comp.slang" },
        .pName = "Renderer.RT.Spatial",
    });
    atrousPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_atrous.comp.slang" },
        .pName = "Renderer.RT.Atrous",
    });
    depthCopyPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSCopyDepth" } }, .pFileName = "RTDenoise.slang" },
        .pName = "Renderer.RT.DepthHistory",
    });
    reflectionPipeline = context.createRayTracingPipeline({
        .pFileName = "RTReflection.slang",
        .pRayGeneration = "rayGenMain",
        .missShaders = { "missMain" },
        .hitGroups = { { .pName = "ReflectionHit", .pClosestHit = "closestHitMain", .pAnyHit = "anyHitFoliage" } },
        .payloadSize = 16,
        .maxRecursionDepth = 2,
        .pName = "Renderer.RT.Reflection",
    });
    ASSERT(sampler.isValid() && materialSampler.isValid() && shadowPipeline.isValid() && temporalPipeline.isValid() &&
           spatialPipeline.isValid() && atrousPipeline.isValid() && depthCopyPipeline.isValid() && reflectionPipeline.isValid());
}

RayTracedEffects::~RayTracedEffects() = default;

void RayTracedEffects::resize(uint32_t width, uint32_t height)
{
    if (!supported)
    {
        return;
    }
    ASSERT(width && height);
    this->width = width;
    this->height = height;
    const DescriptorType views = DescriptorType(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    hz::TextureDesc      desc = { .width = width,
                                  .height = height,
                                  .format = hz::Format::R8G8_UNORM,
                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                  .descriptors = views,
                                  .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                  .pName = "Renderer.RT.ShadowAO" };
    result = context.createTexture(desc);
    desc.pName = "Renderer.RT.DenoiseScratch";
    scratch = context.createTexture(desc);
    desc.pName = "Renderer.RT.History";
    for (hz::GPUTexture& texture : history)
    {
        texture = context.createTexture(desc);
    }
    desc.format = hz::Format::R32_SFLOAT;
    desc.pName = "Renderer.RT.PreviousDepth";
    previousDepth = context.createTexture(desc);
    desc.format = hz::Format::R32G32B32A32_SFLOAT;
    desc.pName = "Renderer.RT.Reflection";
    reflection = context.createTexture(desc);
    if (!blueNoise.isValid())
    {
        blueNoise = context.loadTexture(RD_SHADER_SOURCES, "../Assets/blue_noise_128x128x64.dds");
    }
    historyIndex = 0;
    markDirty();
    ASSERT(result.isValid() && scratch.isValid() && history[0].isValid() && history[1].isValid() && previousDepth.isValid() &&
           reflection.isValid() && blueNoise.isValid() && blueNoise.getWidth() == 128 && blueNoise.getHeight() == 8192);
}

void RayTracedEffects::markDirty() { bMarkedDirty = true; }

void RayTracedEffects::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const DeferredGBuffer& gbuffer, const SceneView& view,
                               const GpuDrawList& draws, const LightingResources& lighting)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RayTracedEffects::execute");
    if (!supported)
    {
        return;
    }
    ASSERT(width && height && gbuffer.pNormalMaterial && gbuffer.pDepth && lighting.pLightingFrame);
    if (!(settings == previousSettings))
    {
        markDirty();
    }
    previousSettings = settings;
    if (assetRevision != view.gpuScene.getAssetTables().revision)
    {
        markDirty();
    }
    assetRevision = view.gpuScene.getAssetTables().revision;
    commands.beginGpuTimestamp("RT Acceleration Structures");
    rayScene->update(commands, view);
    commands.endGpuTimestamp();
    const hz::GPUAccelerationStructure& tlas = rayScene->getSceneTLAS(view.frameIndex);
    const hz::GPUBuffer&                drawData = draws.getDrawData(view.frameIndex);
    auto        index = [](const hz::GPUTexture* texture) { return texture && texture->isValid() ? texture->getSrvIndex() : UINT32_MAX; };
    RTConstants                         constants = {
                                .frame = frame.getSrvIndex(),
                                .lighting = lighting.pLightingFrame->getSrvIndex(),
                                .draws = drawData.getSrvIndex(),
                                .materials = view.gpuScene.getBuffers().materials.getSrvIndex(),
                                .textures = view.gpuScene.getBuffers().textures.getSrvIndex(),
                                .sampler = materialSampler.getIndex(),
                                .tlas = tlas.isValid() ? tlas.getSrvIndex() : UINT32_MAX,
                                .normal = gbuffer.pNormalMaterial->getSrvIndex(),
                                .depth = gbuffer.pDepth->getSrvIndex(),
                                .output = result.getUavIndex(),
                                .blueNoise = blueNoise.getSrvIndex(),
                                .environment = index(lighting.pEnvironment),
                                .irradiance = index(lighting.pIrradiance),
                                .brdf = index(lighting.pBRDFLut),
                                .environmentSampler = lighting.pEnvironmentSampler ? lighting.pEnvironmentSampler->getIndex() : UINT32_MAX,
                                .shadows = settings.shadows,
                                .ao = settings.ao,
                                .aoSamples = TF_MAX(settings.aoSamples, 1u),
                                .frameNumber = frameIndex++,
                                .alphaMethod = settings.shadowAlphaTestMethod,
                                .retraceDepth = settings.retraceMaxDepth,
                                .aoRadius = settings.aoRadius,
                                .aoPower = settings.aoPower,
                                .shadowRadius = settings.shadowRadius,
    };
    hz::Array<const hz::GPUBuffer*>& buffers = rayScene->buffers;
    rayScene->addBuffer(&frame);
    rayScene->addBuffer(lighting.pLightingFrame);
    rayScene->addBuffer(&drawData);
    rayScene->addBuffer(&view.gpuScene.getBuffers().materials);
    rayScene->addBuffer(&view.gpuScene.getBuffers().textures);
    hz::Array<const hz::GPUTexture*>& textures = rayScene->textures;
    textures.pushBack(gbuffer.pNormalMaterial);
    textures.pushBack(gbuffer.pDepth);
    textures.pushBack(&blueNoise);
    commands.beginGpuTimestamp("RT Shadow AO");
    commands.setPipeline(shadowPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { textures.data(), textures.size() },
                        .storageTextures = { &result },
                        .buffers = { buffers.data(), buffers.size() } });
    commands.endGpuTimestamp();

    commands.beginGpuTimestamp("RT Denoise");
    const uint32_t     current = historyIndex;
    const uint32_t     previous = 1 - current;
    RTDenoiseConstants denoise = {
        .frame = frame.getSrvIndex(),
        .input = result.getSrvIndex(),
        .history = history[previous].getSrvIndex(),
        .depth = gbuffer.pDepth->getSrvIndex(),
        .previousDepth = previousDepth.getSrvIndex(),
        .output = history[current].getUavIndex(),
        .sampler = sampler.getIndex(),
        .historyValid = !bMarkedDirty,
        .alpha = settings.temporalAlpha,
        .depthThreshold = settings.temporalDepthThreshold,
    };
    commands.setPipeline(temporalPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&denoise), sizeof(denoise) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { &result, &history[previous], gbuffer.pDepth, &previousDepth },
                        .storageTextures = { &history[current] },
                        .buffers = { &frame } });
    const bool atrous = settings.denoiseMode == RTDenoiseMode::Atrous;
    denoise.depthThreshold = atrous ? settings.atrousDepthSigma : settings.spatialDepthThreshold;
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        const hz::GPUTexture& input = pass ? scratch : history[current];
        const hz::GPUTexture& output = pass ? result : scratch;
        denoise.input = input.getSrvIndex();
        denoise.output = output.getUavIndex();
        denoise.horizontal = pass == 0;
        denoise.step = pass + 1;
        commands.setPipeline(atrous ? atrousPipeline : spatialPipeline);
        commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&denoise), sizeof(denoise) / 4 });
        commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                          { .sampledTextures = { &input, gbuffer.pDepth }, .storageTextures = { &output } });
    }
    denoise.output = previousDepth.getUavIndex();
    commands.setPipeline(depthCopyPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&denoise), sizeof(denoise) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { gbuffer.pDepth }, .storageTextures = { &previousDepth } });
    historyIndex = previous;
    bMarkedDirty = false;
    commands.endGpuTimestamp();

    commands.beginGpuTimestamp("RT Reflection");
    if (lighting.pEnvironment)
    {
        textures.pushBack(lighting.pEnvironment);
    }
    if (lighting.pIrradiance)
    {
        textures.pushBack(lighting.pIrradiance);
    }
    if (lighting.pBRDFLut)
    {
        textures.pushBack(lighting.pBRDFLut);
    }
    constants.output = reflection.getUavIndex();
    if (!settings.reflection)
    {
        constants.tlas = UINT32_MAX;
    }
    commands.setPipeline(reflectionPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
    commands.traceRays(reflectionPipeline, width, height, 1,
                       { .sampledTextures = { textures.data(), textures.size() },
                         .storageTextures = { &reflection },
                         .buffers = { buffers.data(), buffers.size() } });
    commands.endGpuTimestamp();
}
