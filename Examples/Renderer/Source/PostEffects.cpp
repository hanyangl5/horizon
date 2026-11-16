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
    Sampler = Context.CreateSampler(
        { .AddressU = AddressModeClampToEdge, .AddressV = AddressModeClampToEdge, .AddressW = AddressModeClampToEdge });
    MeteringPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain" } }, .pFileName = "PostMetering.slang" },
        .Name = "Renderer.HDR.Metering",
    });
    ReducePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSReduce" } }, .pFileName = "PostLuminance.slang" },
        .Name = "Renderer.HDR.ReduceLuminance",
    });
    InitializePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSInitialize" } }, .pFileName = "PostLuminance.slang" },
        .Name = "Renderer.HDR.InitializeExposure",
    });
    AdaptationPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSAdapt" } }, .pFileName = "PostLuminance.slang" },
        .Name = "Renderer.HDR.Adaptation",
    });
    BloomDownsamplePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomDownsample.slang" },
        .Name = "Renderer.HDR.BloomDownsample",
    });
    BloomUpsamplePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomUpsample.slang" },
        .Name = "Renderer.HDR.BloomUpsample",
    });
    ASSERT(Sampler.IsValid() && MeteringPipeline.IsValid() && ReducePipeline.IsValid() && InitializePipeline.IsValid() &&
           AdaptationPipeline.IsValid() && BloomDownsamplePipeline.IsValid() && BloomUpsamplePipeline.IsValid());
}

bool PostEffects::PrepareBloomChain()
{
    const DescriptorType Views = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture);
    bool                 Valid = true;
    for (uint32_t Level = 0; Level < BloomLevels; ++Level)
    {
        const hz::TextureDesc Desc = { .Width = BloomLevelSize(BloomWidth, Level),
                                       .Height = BloomLevelSize(BloomHeight, Level),
                                       .Format = hz::Format::R16G16B16A16_SFLOAT,
                                       .StartState = ResourceStateUnorderedAccess,
                                       .Descriptors = Views,
                                       .Flags = TextureCreationFlagForce2D,
                                       .Name = "Renderer.HDR.Bloom" };
        BloomDown[Level] = Context.AcquireTransient(Desc);
        Valid &= BloomDown[Level] != nullptr;
        if (Level + 1 < BloomLevels)
        {
            // The smallest level is the base of the accumulation and only needs its downsample.
            BloomUp[Level] = Context.AcquireTransient(Desc);
            Valid &= BloomUp[Level] != nullptr;
        }
    }
    if (!Valid)
    {
        LOGF(Warning, "Bloom targets unavailable; skipping bloom for this frame");
        ReleaseBloomChain();
    }
    return Valid;
}

void PostEffects::ReleaseBloomChain()
{
    for (uint32_t Level = 0; Level < BloomLevels; ++Level)
    {
        if (BloomDown[Level])
            Context.ReleaseTransient(BloomDown[Level]);
        BloomDown[Level] = nullptr;
        if (Level + 1 < BloomLevels)
        {
            if (BloomUp[Level])
                Context.ReleaseTransient(BloomUp[Level]);
            BloomUp[Level] = nullptr;
        }
    }
}

void PostEffects::Resize(uint32_t Width, uint32_t Height)
{
    ASSERT(Width && Height);
    this->Width = Width;
    this->Height = Height;
    const DescriptorType Views = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture);
    // Bloom follows the render Size so the glare keeps its screen-space radius instead of a fixed 512 texel image.
    BloomWidth = Width > 1 ? Width / 2 : 1;
    BloomHeight = Height > 1 ? Height / 2 : 1;
    hz::TextureDesc Desc = { .Format = hz::Format::R16G16B16A16_SFLOAT,
                             .StartState = ResourceStateUnorderedAccess,
                             .Descriptors = Views,
                             .Flags = TextureCreationFlagForce2D };
    bool            Valid = true;
    // Parked targets of the previous Size never match again, so drop them once the GPU is idle.
    Context.PurgeTransients();
    // Independent images let Dependencies describe every reduction without mixed SRV/UAV mip states.
    Desc.Format = hz::Format::R16_SFLOAT;
    Desc.Width = Desc.Height = MeteringSize;
    Desc.Name = "Renderer.HDR.LogLuminance";
    for (hz::GPUTexture& Texture : Luminance)
    {
        Texture = Context.CreateTexture(Desc);
        Valid &= Texture.IsValid();
        Desc.Width = Desc.Width > 1 ? Desc.Width / 2 : 1;
        Desc.Height = Desc.Height > 1 ? Desc.Height / 2 : 1;
    }
    Desc.Width = Desc.Height = 1;
    Desc.Format = hz::Format::R32_SFLOAT;
    Desc.Name = "Renderer.HDR.AdaptedLuminance";
    for (hz::GPUTexture& Texture : AdaptedLuminance)
    {
        Texture = Context.CreateTexture(Desc);
        Valid &= Texture.IsValid();
    }
    if (!ToneMapPipeline.IsValid() || ColorFormat != Context.GetColorFormat())
    {
        ColorFormat = Context.GetColorFormat();
        ToneMapPipeline = Context.CreateGraphicsPipeline({
            .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                        { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
                            .pFileName = "PostToneMap.slang" },
            .colorTargets = { { .Format = Context.GetColorFormat() } },
            .Name = "Renderer.HDR.ToneMap",
        });
    }
    AdaptedIndex = 0;
    ExposureInitialized = false;
    ASSERT(Valid && ToneMapPipeline.IsValid());
}

void PostEffects::Execute(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget,
                          float DeltaSeconds)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("PostEffects::execute");
    ASSERT(SceneColor.IsValid() && RenderTarget.IsValid() && Width && Height);
    Commands.BeginGpuTimestamp("HDR");
    if (!ExposureInitialized)
    {
        hz::DependencyList initializeDependencies;
        const PostLuminanceConstants Initialize = { .inputIndex = initializeDependencies.store(&AdaptedLuminance[0]),
                                                    .outputIndex = initializeDependencies.store(&AdaptedLuminance[1]) };
        Commands.SetPipeline(InitializePipeline);
        Commands.SetRootConstants(Initialize);
        Commands.Dispatch(1, 1, 1, initializeDependencies.Get());
        ExposureInitialized = true;
    }
    hz::DependencyList         MeteringDependencies;
    const PostMeteringConstants Metering = { .sceneColorIndex = MeteringDependencies.sample(&SceneColor),
                                             .luminanceOutputIndex = MeteringDependencies.store(&Luminance[0]) };
    Commands.SetPipeline(MeteringPipeline);
    Commands.SetRootConstants(Metering);
    Commands.Dispatch(MeteringSize / 16, MeteringSize / 16, 1, MeteringDependencies.Get());
    Commands.SetPipeline(ReducePipeline);
    for (uint32_t Level = 1; Level < LuminanceLevels; ++Level)
    {
        const uint32_t Size = MeteringSize >> Level;
        hz::DependencyList reduceDependencies;
        const PostLuminanceConstants Reduce = { .inputIndex = reduceDependencies.sample(&Luminance[Level - 1]),
                                                .outputIndex = reduceDependencies.store(&Luminance[Level]) };
        Commands.SetRootConstants(Reduce);
        Commands.Dispatch((Size + 15) / 16, (Size + 15) / 16, 1, reduceDependencies.Get());
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
    Commands.EndGpuTimestamp();
}

void PostEffects::RunBloom(hz::CommandList& Commands, const hz::GPUTexture& SceneColor)
{
    if (!Settings.EnableBloom || !BloomDown[0])
    {
        return;
    }
    Commands.BeginGpuTimestamp("Bloom");
    Commands.SetPipeline(BloomDownsamplePipeline);
    PostBloomDownsampleConstants Downsample = {
        .input = SceneColor.GetSrvIndex(),
        .output = BloomDown[0]->GetUavIndex(),
        .sampler = Sampler.GetIndex(),
        .adaptedLuminance = AdaptedLuminance[AdaptedIndex].GetSrvIndex(),
        .threshold = 1,
        .autoExposure = Settings.ToneMapping != ToneMappingMode::None ? 1u : 0u,
        .exposure = Settings.Exposure,
    };
    Commands.SetRootConstants(Downsample);
    Commands.Dispatch((BloomWidth + 15) / 16, (BloomHeight + 15) / 16, 1,
                      { .sampledTextures = { &SceneColor, &AdaptedLuminance[AdaptedIndex] }, .storageTextures = { BloomDown[0] } });
    // Only the scene downsample is thresholded; the mip levels already carry bright values.
    Downsample.threshold = 0;
    for (uint32_t Level = 1; Level < BloomLevels; ++Level)
    {
        Downsample.input = BloomDown[Level - 1]->GetSrvIndex();
        Downsample.output = BloomDown[Level]->GetUavIndex();
        Commands.SetRootConstants(Downsample);
        const uint32_t Size = BloomLevelSize(BloomWidth, Level);
        Commands.Dispatch((Size + 15) / 16, (BloomLevelSize(BloomHeight, Level) + 15) / 16, 1,
                          { .sampledTextures = { BloomDown[Level - 1] }, .storageTextures = { BloomDown[Level] } });
    }
    // up[level] = down[level] + tent(up[level + 1]); the smallest downsample seeds the accumulation.
    Commands.SetPipeline(BloomUpsamplePipeline);
    const hz::GPUTexture* Input = BloomDown[BloomLevels - 1];
    for (uint32_t Level = BloomLevels - 1; Level > 0;)
    {
        --Level;
        const PostBloomUpsampleConstants Upsample = { .down = BloomDown[Level]->GetSrvIndex(),
                                                      .input = Input->GetSrvIndex(),
                                                      .output = BloomUp[Level]->GetUavIndex(),
                                                      .sampler = Sampler.GetIndex() };
        Commands.SetRootConstants(Upsample);
        const uint32_t Size = BloomLevelSize(BloomWidth, Level);
        Commands.Dispatch((Size + 15) / 16, (BloomLevelSize(BloomHeight, Level) + 15) / 16, 1,
                          { .sampledTextures = { BloomDown[Level], Input }, .storageTextures = { BloomUp[Level] } });
        Input = BloomUp[Level];
    }
    Commands.EndGpuTimestamp();
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
    Commands.SetPipeline(AdaptationPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch(1, 1, 1, Dependencies.Get());
}

void PostEffects::ToneMap(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget)
{
    const bool SrgbTarget = Context.GetColorFormat() == hz::Format::R8G8B8A8_SRGB || Context.GetColorFormat() == hz::Format::B8G8R8A8_SRGB;
    hz::DependencyList     Dependencies;
    const hz::GPUTexture*  Bloom = BloomUp[0];
    const PostToneConstants Constants = {
        .sceneColor = Dependencies.sample(&SceneColor),
        .adaptedLuminance = Dependencies.sample(&AdaptedLuminance[AdaptedIndex]),
        .bloom = Dependencies.sample(Bloom),
        .sampler = Sampler.GetIndex(),
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
        .outputMode = (uint32_t)Context.GetOutputMode(),
        .encodeSRGB = Context.GetOutputMode() == hz::OUTPUT_MODE_SDR && !SrgbTarget ? 1u : 0u,
        .paperWhiteNits = fmaxf(Settings.PaperWhiteNits, 1.0f),
        .peakNits = Context.IsHDREnabled() ? Context.GetHDRMetadata().maxContentLightLevel : 100.0f,
    };
    Commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &RenderTarget, .loadAction = LoadActionDontCare, .storeAction = StoreActionStore } } },
        Dependencies.Get());
    Commands.SetViewport(0, 0, (float)Width, (float)Height);
    Commands.SetScissor(0, 0, Width, Height);
    Commands.SetPipeline(ToneMapPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Draw(3);
    Commands.EndRendering();
}
