#include "PostEffects.h"
#include "Profiler/IProfiler.h"

#include <math.h>

struct PostBrightConstants
{
    uint32_t sceneColor;
    uint32_t brightOutput;
    uint32_t luminanceOutput;
    uint32_t sampler;
    float    exposure;
    uint32_t enableBloom;
    uint32_t adaptedLuminance;
    uint32_t autoExposure;
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

PostEffects::PostEffects(hz::RenderContext& context): context(context)
{
    sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    brightPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBrightPass.slang" },
        .pName = "Renderer.HDR.BrightPass",
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
    bloomPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloom.slang" },
        .pName = "Renderer.HDR.Bloom",
    });
    ASSERT(sampler.isValid() && brightPipeline.isValid() && reducePipeline.isValid() && initializePipeline.isValid() &&
           adaptationPipeline.isValid() && bloomPipeline.isValid());
}

void PostEffects::resize(uint32_t width, uint32_t height)
{
    ASSERT(width && height);
    this->width = width;
    this->height = height;
    const DescriptorType views = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    hz::TextureDesc      desc = { .width = bloomSize,
                                  .height = bloomSize,
                                  .format = hz::Format::R32G32B32A32_SFLOAT,
                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                  .descriptors = views,
                                  .flags = TEXTURE_CREATION_FLAG_FORCE_2D }; // The luminance chain ends at a 1x1 Texture2D.
    desc.pName = "Renderer.HDR.BrightPass";
    brightPass = context.createTexture(desc);
    desc.pName = "Renderer.HDR.Bloom";
    bloomPass = context.createTexture(desc);
    for (hz::GPUTexture& texture : bloom)
    {
        desc.pName = "Renderer.HDR.BloomScratch";
        texture = context.createTexture(desc);
    }
    // Independent images let Dependencies describe every reduction without mixed SRV/UAV mip states.
    desc.format = hz::Format::R16_SFLOAT;
    desc.pName = "Renderer.HDR.LogLuminance";
    bool valid = brightPass.isValid() && bloomPass.isValid() && bloom[0].isValid() && bloom[1].isValid();
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
    const PostBrightConstants constants = {
        .sceneColor = sceneColor.getSrvIndex(),
        .brightOutput = brightPass.getUavIndex(),
        .luminanceOutput = luminance[0].getUavIndex(),
        .sampler = sampler.getIndex(),
        .exposure = settings.exposure,
        .enableBloom = settings.enableBloom && settings.bloomPasses ? 1u : 0u,
        .adaptedLuminance = adaptedLuminance[adaptedIndex].getSrvIndex(),
        .autoExposure = settings.toneMapping != ToneMappingMode::None ? 1u : 0u,
    };
    commands.setPipeline(brightPipeline);
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
    commands.dispatch(
        bloomSize / 16, bloomSize / 16, 1,
        { .sampledTextures = { &sceneColor, &adaptedLuminance[adaptedIndex] }, .storageTextures = { &brightPass, &luminance[0] } });
    commands.setPipeline(reducePipeline);
    for (uint32_t level = 1; level < luminanceLevels; ++level)
    {
        const uint32_t size = bloomSize >> level;
        commands.setRootConstant({ luminance[level - 1].getSrvIndex(), luminance[level].getUavIndex(), 0, 0 });
        commands.dispatch((size + 15) / 16, (size + 15) / 16, 1,
                          { .sampledTextures = { &luminance[level - 1] }, .storageTextures = { &luminance[level] } });
    }
    runBloom(commands);
    runAdaptation(commands, deltaSeconds);
    toneMap(commands, sceneColor, renderTarget);
    // Match the reference: this frame meters into the other image; its exposure is used next frame.
    adaptedIndex ^= 1;
    commands.endGpuTimestamp();
}

void PostEffects::runBloom(hz::CommandList& commands)
{
    if (!settings.enableBloom || !settings.bloomPasses)
    {
        return;
    }
    commands.beginGpuTimestamp("Bloom");
    commands.setPipeline(bloomPipeline);
    const hz::GPUTexture* input = &brightPass;
    const uint32_t        passCount = settings.bloomPasses * 2;
    for (uint32_t pass = 0; pass < passCount; ++pass)
    {
        const hz::GPUTexture* output = pass + 1 == passCount ? &bloomPass : &bloom[pass & 1];
        commands.setRootConstant({ input->getSrvIndex(), output->getUavIndex(), sampler.getIndex(), pass & 1 });
        commands.dispatch(bloomSize / 16, bloomSize / 16, 1, { .sampledTextures = { input }, .storageTextures = { output } });
        input = output;
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
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
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
        .bloom = bloomPass.getSrvIndex(),
        .sampler = sampler.getIndex(),
        .toneMapping = (uint32_t)settings.toneMapping,
        .exposure = settings.exposure,
        .bloomStrength = settings.enableBloom && settings.bloomPasses ? settings.bloomStrength : 0.0f,
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
        { .sampledTextures = { &sceneColor, &adaptedLuminance[adaptedIndex], &bloomPass } });
    commands.setViewport(0, 0, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(toneMapPipeline);
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
    commands.draw(3);
    commands.endRendering();
}
