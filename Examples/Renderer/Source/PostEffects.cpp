#include "PostEffects.h"
#include "Core/ILog.h"
#include "Profiler/IProfiler.h"

#include <math.h>

// Mirrors the RootConstant0 block of PostMetering.slang.
struct PostMeteringConstants
{
    uint32_t sceneColorIndex;
    uint32_t luminanceOutputIndex;
};
static_assert(sizeof(PostMeteringConstants) == 2 * sizeof(uint32_t));

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

// Mirrors the RootConstant0 block that every PostLuminance.slang entry point shares.
struct PostLuminanceConstants
{
    uint32_t inputIndex;
    uint32_t outputIndex;
    uint32_t previousIndex = 0;
    float    adaptationTime = 0.0f;
};
static_assert(sizeof(PostLuminanceConstants) == 3 * sizeof(uint32_t) + sizeof(float));

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

static uint32_t BloomLevelSize(uint32_t Size, uint32_t Level)
{
    const uint32_t Shifted = Size >> Level;
    return Shifted ? Shifted : 1;
}

PostEffects::PostEffects(hz::RenderContext& Context): Context(Context)
{
    Sampler = Context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    MeteringPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostMetering.slang" },
        .pName = "Renderer.HDR.Metering",
    });
    ReducePipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSReduce" } }, .pFileName = "PostLuminance.slang" },
        .pName = "Renderer.HDR.ReduceLuminance",
    });
    InitializePipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSInitialize" } }, .pFileName = "PostLuminance.slang" },
        .pName = "Renderer.HDR.InitializeExposure",
    });
    AdaptationPipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSAdapt" } }, .pFileName = "PostLuminance.slang" },
        .pName = "Renderer.HDR.Adaptation",
    });
    BloomDownsamplePipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomDownsample.slang" },
        .pName = "Renderer.HDR.BloomDownsample",
    });
    BloomUpsamplePipeline = Context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomUpsample.slang" },
        .pName = "Renderer.HDR.BloomUpsample",
    });
    ASSERT(Sampler.isValid() && MeteringPipeline.isValid() && ReducePipeline.isValid() && InitializePipeline.isValid() &&
           AdaptationPipeline.isValid() && BloomDownsamplePipeline.isValid() && BloomUpsamplePipeline.isValid());
}

bool PostEffects::PrepareBloomChain()
{
    const DescriptorType Views = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    bool                 Valid = true;
    for (uint32_t Level = 0; Level < BloomLevels; ++Level)
    {
        const hz::TextureDesc Desc = { .width = BloomLevelSize(BloomWidth, Level),
                                       .height = BloomLevelSize(BloomHeight, Level),
                                       .format = hz::Format::R16G16B16A16_SFLOAT,
                                       .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                       .descriptors = Views,
                                       .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .pName = "Renderer.HDR.Bloom" };
        BloomDown[Level] = Context.acquireTransient(Desc);
        Valid &= BloomDown[Level] != nullptr;
        if (Level + 1 < BloomLevels)
        {
            // The smallest level is the base of the accumulation and only needs its downsample.
            BloomUp[Level] = Context.acquireTransient(Desc);
            Valid &= BloomUp[Level] != nullptr;
        }
    }
    if (!Valid)
    {
        LOGF(eWARNING, "Bloom targets unavailable; skipping bloom for this frame");
        ReleaseBloomChain();
    }
    return Valid;
}

void PostEffects::ReleaseBloomChain()
{
    for (uint32_t Level = 0; Level < BloomLevels; ++Level)
    {
        if (BloomDown[Level])
            Context.releaseTransient(BloomDown[Level]);
        BloomDown[Level] = nullptr;
        if (Level + 1 < BloomLevels)
        {
            if (BloomUp[Level])
                Context.releaseTransient(BloomUp[Level]);
            BloomUp[Level] = nullptr;
        }
    }
}

void PostEffects::Resize(uint32_t Width, uint32_t Height)
{
    ASSERT(Width && Height);
    this->Width = Width;
    this->Height = Height;
    const DescriptorType Views = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    // Bloom follows the render size so the glare keeps its screen-space radius instead of a fixed 512 texel image.
    BloomWidth = Width > 1 ? Width / 2 : 1;
    BloomHeight = Height > 1 ? Height / 2 : 1;
    hz::TextureDesc Desc = { .format = hz::Format::R16G16B16A16_SFLOAT,
                             .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                             .descriptors = Views,
                             .flags = TEXTURE_CREATION_FLAG_FORCE_2D };
    bool            Valid = true;
    // Parked targets of the previous size never match again, so drop them once the GPU is idle.
    Context.purgeTransients();
    // Independent images let Dependencies describe every reduction without mixed SRV/UAV mip states.
    Desc.format = hz::Format::R16_SFLOAT;
    Desc.width = Desc.height = MeteringSize;
    Desc.pName = "Renderer.HDR.LogLuminance";
    for (hz::GPUTexture& Texture : Luminance)
    {
        Texture = Context.createTexture(Desc);
        Valid &= Texture.isValid();
        Desc.width = Desc.width > 1 ? Desc.width / 2 : 1;
        Desc.height = Desc.height > 1 ? Desc.height / 2 : 1;
    }
    Desc.width = Desc.height = 1;
    Desc.format = hz::Format::R32_SFLOAT;
    Desc.pName = "Renderer.HDR.AdaptedLuminance";
    for (hz::GPUTexture& Texture : AdaptedLuminance)
    {
        Texture = Context.createTexture(Desc);
        Valid &= Texture.isValid();
    }
    if (!ToneMapPipeline.isValid() || ColorFormat != Context.getColorFormat())
    {
        ColorFormat = Context.getColorFormat();
        ToneMapPipeline = Context.createGraphicsPipeline({
            .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                        { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                            .pFileName = "PostToneMap.slang" },
            .colorTargets = { { .format = Context.getColorFormat() } },
            .pName = "Renderer.HDR.ToneMap",
        });
    }
    AdaptedIndex = 0;
    ExposureInitialized = false;
    ASSERT(Valid && ToneMapPipeline.isValid());
}

void PostEffects::Execute(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget,
                          float DeltaSeconds)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("PostEffects::Execute");
    ASSERT(SceneColor.isValid() && RenderTarget.isValid() && Width && Height);
    Commands.beginGpuTimestamp("HDR");
    if (!ExposureInitialized)
    {
        hz::DependencyList initializeDependencies;
        const PostLuminanceConstants Initialize = { .inputIndex = initializeDependencies.store(&AdaptedLuminance[0]),
                                                    .outputIndex = initializeDependencies.store(&AdaptedLuminance[1]) };
        Commands.setPipeline(InitializePipeline);
        Commands.setRootConstants(Initialize);
        Commands.dispatch(1, 1, 1, initializeDependencies.get());
        ExposureInitialized = true;
    }
    hz::DependencyList         MeteringDependencies;
    const PostMeteringConstants Metering = { .sceneColorIndex = MeteringDependencies.sample(&SceneColor),
                                             .luminanceOutputIndex = MeteringDependencies.store(&Luminance[0]) };
    Commands.setPipeline(MeteringPipeline);
    Commands.setRootConstants(Metering);
    Commands.dispatch(MeteringSize / 16, MeteringSize / 16, 1, MeteringDependencies.get());
    Commands.setPipeline(ReducePipeline);
    for (uint32_t Level = 1; Level < LuminanceLevels; ++Level)
    {
        const uint32_t Size = MeteringSize >> Level;
        hz::DependencyList reduceDependencies;
        const PostLuminanceConstants Reduce = { .inputIndex = reduceDependencies.sample(&Luminance[Level - 1]),
                                                .outputIndex = reduceDependencies.store(&Luminance[Level]) };
        Commands.setRootConstants(Reduce);
        Commands.dispatch((Size + 15) / 16, (Size + 15) / 16, 1, reduceDependencies.get());
    }
    const bool BloomReady = PrepareBloomChain();
    if (BloomReady)
    {
        RunBloom(Commands, SceneColor);
    }
    RunAdaptation(Commands, DeltaSeconds);
    ToneMap(Commands, SceneColor, RenderTarget);
    ReleaseBloomChain();
    // Match the reference: this frame meters into the other image; its exposure is used next frame.
    AdaptedIndex ^= 1;
    Commands.endGpuTimestamp();
}

void PostEffects::RunBloom(hz::CommandList& Commands, const hz::GPUTexture& SceneColor)
{
    if (!Settings.EnableBloom || !BloomDown[0])
    {
        return;
    }
    Commands.beginGpuTimestamp("Bloom");
    Commands.setPipeline(BloomDownsamplePipeline);
    PostBloomDownsampleConstants Downsample = {
        .input = SceneColor.getSrvIndex(),
        .output = BloomDown[0]->getUavIndex(),
        .sampler = Sampler.getIndex(),
        .adaptedLuminance = AdaptedLuminance[AdaptedIndex].getSrvIndex(),
        .threshold = 1,
        .autoExposure = Settings.ToneMapping != ToneMappingMode::None ? 1u : 0u,
        .exposure = Settings.Exposure,
    };
    Commands.setRootConstants(Downsample);
    Commands.dispatch((BloomWidth + 15) / 16, (BloomHeight + 15) / 16, 1,
                      { .sampledTextures = { &SceneColor, &AdaptedLuminance[AdaptedIndex] }, .storageTextures = { BloomDown[0] } });
    // Only the scene downsample is thresholded; the mip levels already carry bright values.
    Downsample.threshold = 0;
    for (uint32_t Level = 1; Level < BloomLevels; ++Level)
    {
        Downsample.input = BloomDown[Level - 1]->getSrvIndex();
        Downsample.output = BloomDown[Level]->getUavIndex();
        Commands.setRootConstants(Downsample);
        const uint32_t Size = BloomLevelSize(BloomWidth, Level);
        Commands.dispatch((Size + 15) / 16, (BloomLevelSize(BloomHeight, Level) + 15) / 16, 1,
                          { .sampledTextures = { BloomDown[Level - 1] }, .storageTextures = { BloomDown[Level] } });
    }
    // up[level] = down[level] + tent(up[level + 1]); the smallest downsample seeds the accumulation.
    Commands.setPipeline(BloomUpsamplePipeline);
    const hz::GPUTexture* Input = BloomDown[BloomLevels - 1];
    for (uint32_t Level = BloomLevels - 1; Level > 0;)
    {
        --Level;
        const PostBloomUpsampleConstants Upsample = { .down = BloomDown[Level]->getSrvIndex(),
                                                      .input = Input->getSrvIndex(),
                                                      .output = BloomUp[Level]->getUavIndex(),
                                                      .sampler = Sampler.getIndex() };
        Commands.setRootConstants(Upsample);
        const uint32_t Size = BloomLevelSize(BloomWidth, Level);
        Commands.dispatch((Size + 15) / 16, (BloomLevelSize(BloomHeight, Level) + 15) / 16, 1,
                          { .sampledTextures = { BloomDown[Level], Input }, .storageTextures = { BloomUp[Level] } });
        Input = BloomUp[Level];
    }
    Commands.endGpuTimestamp();
}

void PostEffects::RunAdaptation(hz::CommandList& Commands, float DeltaSeconds)
{
    hz::DependencyList           Dependencies;
    const PostLuminanceConstants Constants = {
        .inputIndex = Dependencies.sample(&Luminance[LuminanceLevels - 1]),
        .outputIndex = Dependencies.store(&AdaptedLuminance[AdaptedIndex ^ 1]),
        .previousIndex = Dependencies.sample(&AdaptedLuminance[AdaptedIndex]),
        .adaptationTime = fmaxf(DeltaSeconds, 0.0f) * fmaxf(Settings.AdaptationSpeed, 0.0f),
    };
    Commands.setPipeline(AdaptationPipeline);
    Commands.setRootConstants(Constants);
    Commands.dispatch(1, 1, 1, Dependencies.get());
}

void PostEffects::ToneMap(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget)
{
    const bool SrgbTarget = Context.getColorFormat() == hz::Format::R8G8B8A8_SRGB || Context.getColorFormat() == hz::Format::B8G8R8A8_SRGB;
    hz::DependencyList     Dependencies;
    const hz::GPUTexture*  Bloom = BloomUp[0];
    const PostToneConstants Constants = {
        .sceneColor = Dependencies.sample(&SceneColor),
        .adaptedLuminance = Dependencies.sample(&AdaptedLuminance[AdaptedIndex]),
        .bloom = Dependencies.sample(Bloom),
        .sampler = Sampler.getIndex(),
        .toneMapping = (uint32_t)Settings.ToneMapping,
        .exposure = Settings.Exposure,
        .bloomStrength = Settings.EnableBloom && Bloom ? Settings.BloomStrength : 0.0f,
        .maxWhite = Settings.MaxWhite,
        .P = Settings.UchimuraP,
        .a = Settings.UchimuraA,
        .m = Settings.UchimuraM,
        .l = Settings.UchimuraL,
        .c = Settings.UchimuraC,
        .b = Settings.UchimuraB,
        .startCompression = Settings.StartCompression,
        .desaturation = Settings.Desaturation,
        .outputMode = (uint32_t)Context.getOutputMode(),
        .encodeSRGB = Context.getOutputMode() == hz::OUTPUT_MODE_SDR && !SrgbTarget ? 1u : 0u,
        .paperWhiteNits = fmaxf(Settings.PaperWhiteNits, 1.0f),
        .peakNits = Context.isHDREnabled() ? Context.getHDRMetadata().maxContentLightLevel : 100.0f,
    };
    Commands.beginRendering(
        { .colorAttachments = { { .pTexture = &RenderTarget, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        Dependencies.get());
    Commands.setViewport(0, 0, (float)Width, (float)Height);
    Commands.setScissor(0, 0, Width, Height);
    Commands.setPipeline(ToneMapPipeline);
    Commands.setRootConstants(Constants);
    Commands.draw(3);
    Commands.endRendering();
}
