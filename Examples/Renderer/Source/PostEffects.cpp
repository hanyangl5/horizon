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

void PostEffects::Resize()
{
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
    ASSERT(ToneMapPipeline.IsValid());
}

void PostEffects::Execute(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget,
                          float DeltaSeconds, const PostEffectsTargets& Targets)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("PostEffects::execute");
    ASSERT(SceneColor.IsValid() && RenderTarget.IsValid() && Targets.Adapted && Targets.PreviousAdapted);
    Commands.BeginGpuTimestamp("HDR");
    if (!Targets.AdaptedValid)
    {
        hz::DependencyList initializeDependencies;
        const PostLuminanceConstants Initialize = { .inputIndex = initializeDependencies.store(Targets.PreviousAdapted),
                                                    .outputIndex = initializeDependencies.store(Targets.Adapted) };
        Commands.SetPipeline(InitializePipeline);
        Commands.SetRootConstants(Initialize);
        Commands.Dispatch(1, 1, 1, initializeDependencies.Get());
    }
    constexpr uint32_t          MeteringSize = PostEffectsTargets::MeteringSize;
    hz::DependencyList         MeteringDependencies;
    const PostMeteringConstants Metering = { .sceneColorIndex = MeteringDependencies.sample(&SceneColor),
                                             .luminanceOutputIndex = MeteringDependencies.store(Targets.Luminance[0]) };
    Commands.SetPipeline(MeteringPipeline);
    Commands.SetRootConstants(Metering);
    Commands.Dispatch(MeteringSize / 16, MeteringSize / 16, 1, MeteringDependencies.Get());
    Commands.SetPipeline(ReducePipeline);
    for (uint32_t Level = 1; Level < PostEffectsTargets::LuminanceLevels; ++Level)
    {
        const uint32_t Size = MeteringSize >> Level;
        hz::DependencyList reduceDependencies;
        const PostLuminanceConstants Reduce = { .inputIndex = reduceDependencies.sample(Targets.Luminance[Level - 1]),
                                                .outputIndex = reduceDependencies.store(Targets.Luminance[Level]) };
        Commands.SetRootConstants(Reduce);
        Commands.Dispatch((Size + 15) / 16, (Size + 15) / 16, 1, reduceDependencies.Get());
    }
    RunBloom(Commands, SceneColor, Targets);
    RunAdaptation(Commands, DeltaSeconds, Targets);
    // Match the reference: bloom and tone mapping use last frame's exposure; this frame's is used next frame.
    ToneMap(Commands, SceneColor, RenderTarget, Targets);
    Commands.EndGpuTimestamp();
}

void PostEffects::RunBloom(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const PostEffectsTargets& Targets)
{
    if (!Settings.EnableBloom || !Targets.BloomDown[0])
    {
        return;
    }
    const hz::GPUTexture* const* BloomDown = Targets.BloomDown;
    const hz::GPUTexture* const* BloomUp = Targets.BloomUp;
    const hz::GPUTexture&        Previous = *Targets.PreviousAdapted;
    const uint32_t               BloomWidth = BloomDown[0]->GetWidth();
    const uint32_t               BloomHeight = BloomDown[0]->GetHeight();
    constexpr uint32_t           BloomLevels = PostEffectsTargets::BloomLevels;
    Commands.BeginGpuTimestamp("Bloom");
    Commands.SetPipeline(BloomDownsamplePipeline);
    PostBloomDownsampleConstants Downsample = {
        .input = SceneColor.GetSrvIndex(),
        .output = BloomDown[0]->GetUavIndex(),
        .sampler = Sampler.GetIndex(),
        .adaptedLuminance = Previous.GetSrvIndex(),
        .threshold = 1,
        .autoExposure = Settings.ToneMapping != ToneMappingMode::None ? 1u : 0u,
        .exposure = Settings.Exposure,
    };
    Commands.SetRootConstants(Downsample);
    Commands.Dispatch((BloomWidth + 15) / 16, (BloomHeight + 15) / 16, 1,
                      { .sampledTextures = { &SceneColor, &Previous }, .storageTextures = { BloomDown[0] } });
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

void PostEffects::RunAdaptation(hz::CommandList& Commands, float DeltaSeconds, const PostEffectsTargets& Targets)
{
    hz::DependencyList           Dependencies;
    const PostLuminanceConstants Constants = {
        .inputIndex = Dependencies.sample(Targets.Luminance[PostEffectsTargets::LuminanceLevels - 1]),
        .outputIndex = Dependencies.store(Targets.Adapted),
        .previousIndex = Dependencies.sample(Targets.PreviousAdapted),
        .adaptationTime = fmaxf(DeltaSeconds, 0.0f) * fmaxf(Settings.AdaptationSpeed, 0.0f),
    };
    Commands.SetPipeline(AdaptationPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch(1, 1, 1, Dependencies.Get());
}

void PostEffects::ToneMap(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget,
                          const PostEffectsTargets& Targets)
{
    const uint32_t Width = RenderTarget.GetWidth();
    const uint32_t Height = RenderTarget.GetHeight();
    const bool SrgbTarget = Context.GetColorFormat() == hz::Format::R8G8B8A8_SRGB || Context.GetColorFormat() == hz::Format::B8G8R8A8_SRGB;
    hz::DependencyList     Dependencies;
    const hz::GPUTexture*  Bloom = Settings.EnableBloom ? Targets.BloomUp[0] : nullptr;
    const PostToneConstants Constants = {
        .sceneColor = Dependencies.sample(&SceneColor),
        .adaptedLuminance = Dependencies.sample(Targets.PreviousAdapted),
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
