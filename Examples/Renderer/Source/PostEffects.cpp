#include "PostEffects.h"
#include "Profiler/IProfiler.h"

#include <math.h>

struct PostMeteringConstants
{
    uint32_t sceneColor;
    uint32_t luminanceOutput;
};

struct PostBloomDownsampleConstants
{
    uint32_t input;
    uint32_t output;
    uint32_t sampler;
    uint32_t adaptedLuminance;
    uint32_t threshold;
    uint32_t autoExposure;
    float    exposure;
};

struct PostBloomUpsampleConstants
{
    uint32_t down;
    uint32_t input;
    uint32_t output;
    uint32_t sampler;
};

struct PostMeterConstants
{
    uint32_t input;
    uint32_t output;
    uint32_t previous;
    float    adaptationTime;
};

struct PostToneConstants
{
    uint32_t sceneColor;
    uint32_t adaptedLuminance;
    uint32_t bloom;
    uint32_t sampler;
    uint32_t toneMapping;
    float    exposure;
    float    bloomStrength;
    float    maxWhite;
    float    P;
    float    a;
    float    m;
    float    l;
    float    c;
    float    b;
    float    startCompression;
    float    desaturation;
    uint32_t outputMode;
    uint32_t encodeSRGB;
    float    paperWhiteNits;
    float    peakNits;
};

static uint32_t bloomLevelSize(uint32_t size, uint32_t level)
{
    const uint32_t shifted = size >> level;
    return shifted ? shifted : 1;
}

PostEffects::PostEffects(hz::RenderContext& context): context(context)
{
    sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    meteringPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostMetering.slang" },
        .pName = "Renderer.HDR.Metering",
    });
    reducePipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSReduce" } }, .pFileName = "PostLuminance.slang" },
        .pName = "Renderer.HDR.ReduceLuminance",
    });
    initializePipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSInitialize" } }, .pFileName = "PostLuminance.slang" },
        .pName = "Renderer.HDR.InitializeExposure",
    });
    adaptationPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSAdapt" } }, .pFileName = "PostLuminance.slang" },
        .pName = "Renderer.HDR.Adaptation",
    });
    bloomDownsamplePipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomDownsample.slang" },
        .pName = "Renderer.HDR.BloomDownsample",
    });
    bloomUpsamplePipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomUpsample.slang" },
        .pName = "Renderer.HDR.BloomUpsample",
    });
    ASSERT(sampler.isValid() && meteringPipeline.isValid() && reducePipeline.isValid() && initializePipeline.isValid() &&
           adaptationPipeline.isValid() && bloomDownsamplePipeline.isValid() && bloomUpsamplePipeline.isValid());
}

void PostEffects::acquireBloomChain()
{
    const DescriptorType views = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    for (uint32_t level = 0; level < bloomLevels; ++level)
    {
        const hz::TextureDesc desc = { .width = bloomLevelSize(bloomWidth, level),
                                       .height = bloomLevelSize(bloomHeight, level),
                                       .format = hz::Format::R16G16B16A16_SFLOAT,
                                       .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                       .descriptors = views,
                                       .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .pName = "Renderer.HDR.Bloom" };
        bloomDown[level] = context.acquireTransient(desc);
        if (level + 1 < bloomLevels)
        {
            // The smallest level is the base of the accumulation and only needs its downsample.
            bloomUp[level] = context.acquireTransient(desc);
        }
    }
}

void PostEffects::releaseBloomChain()
{
    for (uint32_t level = 0; level < bloomLevels; ++level)
    {
        context.releaseTransient(bloomDown[level]);
        if (level + 1 < bloomLevels)
            context.releaseTransient(bloomUp[level]);
    }
}

void PostEffects::resize(uint32_t width, uint32_t height)
{
    ASSERT(width && height);
    this->width = width;
    this->height = height;
    const DescriptorType views = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    // Bloom follows the render size so the glare keeps its screen-space radius instead of a fixed 512 texel image.
    bloomWidth = width > 1 ? width / 2 : 1;
    bloomHeight = height > 1 ? height / 2 : 1;
    hz::TextureDesc desc = { .format = hz::Format::R16G16B16A16_SFLOAT,
                             .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                             .descriptors = views,
                             .flags = TEXTURE_CREATION_FLAG_FORCE_2D };
    bool            valid = true;
    // Bloom targets are pooled per frame, so create the chain once here; execute only borrows it.
    acquireBloomChain();
    for (uint32_t level = 0; level < bloomLevels; ++level)
    {
        valid &= bloomDown[level] && bloomDown[level]->isValid();
        if (level + 1 < bloomLevels)
            valid &= bloomUp[level] && bloomUp[level]->isValid();
    }
    releaseBloomChain();
    // Independent images let Dependencies describe every reduction without mixed SRV/UAV mip states.
    desc.format = hz::Format::R16_SFLOAT;
    desc.width = desc.height = meteringSize;
    desc.pName = "Renderer.HDR.LogLuminance";
    for (hz::GPUTexture& texture : luminance)
    {
        texture = context.createTexture(desc);
        valid &= texture.isValid();
        desc.width = desc.width > 1 ? desc.width / 2 : 1;
        desc.height = desc.height > 1 ? desc.height / 2 : 1;
    }
    desc.width = desc.height = 1;
    desc.format = hz::Format::R32_SFLOAT;
    desc.pName = "Renderer.HDR.AdaptedLuminance";
    for (hz::GPUTexture& texture : adaptedLuminance)
    {
        texture = context.createTexture(desc);
        valid &= texture.isValid();
    }
    if (!toneMapPipeline.isValid() || colorFormat != context.getColorFormat())
    {
        colorFormat = context.getColorFormat();
        toneMapPipeline = context.createGraphicsPipeline({
            .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                        { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                            .pFileName = "PostToneMap.slang" },
            .colorTargets = { { .format = context.getColorFormat() } },
            .pName = "Renderer.HDR.ToneMap",
        });
    }
    adaptedIndex = 0;
    exposureInitialized = false;
    ASSERT(valid && toneMapPipeline.isValid());
}

void PostEffects::execute(hz::CommandList& commands, const hz::GPUTexture& sceneColor, const hz::GPUTexture& renderTarget,
                          float deltaSeconds)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("PostEffects::execute");
    ASSERT(sceneColor.isValid() && renderTarget.isValid() && width && height);
    commands.beginGpuTimestamp("HDR");
    if (!exposureInitialized)
    {
        commands.setPipeline(initializePipeline);
        commands.setRootConstant({ adaptedLuminance[0].getUavIndex(), adaptedLuminance[1].getUavIndex(), 0, 0 });
        commands.dispatch(1, 1, 1, { .storageTextures = { &adaptedLuminance[0], &adaptedLuminance[1] } });
        exposureInitialized = true;
    }
    const PostMeteringConstants metering = { .sceneColor = sceneColor.getSrvIndex(), .luminanceOutput = luminance[0].getUavIndex() };
    commands.setPipeline(meteringPipeline);
    commands.setRootConstants(metering);
    commands.dispatch(meteringSize / 16, meteringSize / 16, 1,
                      { .sampledTextures = { &sceneColor }, .storageTextures = { &luminance[0] } });
    commands.setPipeline(reducePipeline);
    for (uint32_t level = 1; level < luminanceLevels; ++level)
    {
        const uint32_t size = meteringSize >> level;
        commands.setRootConstant({ luminance[level - 1].getSrvIndex(), luminance[level].getUavIndex(), 0, 0 });
        commands.dispatch((size + 15) / 16, (size + 15) / 16, 1,
                          { .sampledTextures = { &luminance[level - 1] }, .storageTextures = { &luminance[level] } });
    }
    acquireBloomChain();

    runBloom(commands, sceneColor);
    runAdaptation(commands, deltaSeconds);
    toneMap(commands, sceneColor, renderTarget);
    releaseBloomChain();
    // Match the reference: this frame meters into the other image; its exposure is used next frame.
    adaptedIndex ^= 1;
    commands.endGpuTimestamp();
}

void PostEffects::runBloom(hz::CommandList& commands, const hz::GPUTexture& sceneColor)
{
    if (!settings.enableBloom)
    {
        return;
    }
    commands.beginGpuTimestamp("Bloom");
    commands.setPipeline(bloomDownsamplePipeline);
    PostBloomDownsampleConstants downsample = {
        .input = sceneColor.getSrvIndex(),
        .output = bloomDown[0]->getUavIndex(),
        .sampler = sampler.getIndex(),
        .adaptedLuminance = adaptedLuminance[adaptedIndex].getSrvIndex(),
        .threshold = 1,
        .autoExposure = settings.toneMapping != ToneMappingMode::None ? 1u : 0u,
        .exposure = settings.exposure,
    };
    commands.setRootConstants(downsample);
    commands.dispatch((bloomWidth + 15) / 16, (bloomHeight + 15) / 16, 1,
                      { .sampledTextures = { &sceneColor, &adaptedLuminance[adaptedIndex] }, .storageTextures = { bloomDown[0] } });
    // Only the scene downsample is thresholded; the mip levels already carry bright values.
    downsample.threshold = 0;
    for (uint32_t level = 1; level < bloomLevels; ++level)
    {
        downsample.input = bloomDown[level - 1]->getSrvIndex();
        downsample.output = bloomDown[level]->getUavIndex();
        commands.setRootConstants(downsample);
        const uint32_t size = bloomLevelSize(bloomWidth, level);
        commands.dispatch((size + 15) / 16, (bloomLevelSize(bloomHeight, level) + 15) / 16, 1,
                          { .sampledTextures = { bloomDown[level - 1] }, .storageTextures = { bloomDown[level] } });
    }
    // up[level] = down[level] + tent(up[level + 1]); the smallest downsample seeds the accumulation.
    commands.setPipeline(bloomUpsamplePipeline);
    const hz::GPUTexture* input = bloomDown[bloomLevels - 1];
    for (uint32_t level = bloomLevels - 1; level > 0;)
    {
        --level;
        const PostBloomUpsampleConstants upsample = { .down = bloomDown[level]->getSrvIndex(),
                                                      .input = input->getSrvIndex(),
                                                      .output = bloomUp[level]->getUavIndex(),
                                                      .sampler = sampler.getIndex() };
        commands.setRootConstants(upsample);
        const uint32_t size = bloomLevelSize(bloomWidth, level);
        commands.dispatch((size + 15) / 16, (bloomLevelSize(bloomHeight, level) + 15) / 16, 1,
                          { .sampledTextures = { bloomDown[level], input }, .storageTextures = { bloomUp[level] } });
        input = bloomUp[level];
    }
    commands.endGpuTimestamp();
}

void PostEffects::runAdaptation(hz::CommandList& commands, float deltaSeconds)
{
    const PostMeterConstants constants = {
        .input = luminance[luminanceLevels - 1].getSrvIndex(),
        .output = adaptedLuminance[adaptedIndex ^ 1].getUavIndex(),
        .previous = adaptedLuminance[adaptedIndex].getSrvIndex(),
        .adaptationTime = fmaxf(deltaSeconds, 0.0f) * fmaxf(settings.adaptationSpeed, 0.0f),
    };
    commands.setPipeline(adaptationPipeline);
    commands.setRootConstants(constants);
    commands.dispatch(1, 1, 1,
                      { .sampledTextures = { &luminance[luminanceLevels - 1], &adaptedLuminance[adaptedIndex] },
                        .storageTextures = { &adaptedLuminance[adaptedIndex ^ 1] } });
}

void PostEffects::toneMap(hz::CommandList& commands, const hz::GPUTexture& sceneColor, const hz::GPUTexture& renderTarget)
{
    const bool srgbTarget = context.getColorFormat() == hz::Format::R8G8B8A8_SRGB || context.getColorFormat() == hz::Format::B8G8R8A8_SRGB;
    const PostToneConstants constants = {
        .sceneColor = sceneColor.getSrvIndex(),
        .adaptedLuminance = adaptedLuminance[adaptedIndex].getSrvIndex(),
        .bloom = bloomUp[0]->getSrvIndex(),
        .sampler = sampler.getIndex(),
        .toneMapping = (uint32_t)settings.toneMapping,
        .exposure = settings.exposure,
        .bloomStrength = settings.enableBloom ? settings.bloomStrength : 0.0f,
        .maxWhite = settings.maxWhite,
        .P = settings.uchimuraP,
        .a = settings.uchimuraA,
        .m = settings.uchimuraM,
        .l = settings.uchimuraL,
        .c = settings.uchimuraC,
        .b = settings.uchimuraB,
        .startCompression = settings.startCompression,
        .desaturation = settings.desaturation,
        .outputMode = (uint32_t)context.getOutputMode(),
        .encodeSRGB = context.getOutputMode() == hz::OUTPUT_MODE_SDR && !srgbTarget ? 1u : 0u,
        .paperWhiteNits = fmaxf(settings.paperWhiteNits, 1.0f),
        .peakNits = context.isHDREnabled() ? context.getHDRMetadata().maxContentLightLevel : 100.0f,
    };
    commands.beginRendering(
        { .colorAttachments = { { .pTexture = &renderTarget, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        { .sampledTextures = { &sceneColor, &adaptedLuminance[adaptedIndex], bloomUp[0] } });
    commands.setViewport(0, 0, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(toneMapPipeline);
    commands.setRootConstants(constants);
    commands.draw(3);
    commands.endRendering();
}
