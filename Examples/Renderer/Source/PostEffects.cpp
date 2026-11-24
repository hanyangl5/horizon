#include "Core/ICVar.h"
#include "PostEffects.h"
#include "Core/ILog.h"
#include "Profiler/IProfiler.h"

#include <math.h>

// Mirrors the RootConstant0 block of PostMetering.slang.
struct PostMeteringConstants
{
    uint32_t SceneColorIndex;
    uint32_t LuminanceOutputIndex;
};
static_assert(sizeof(PostMeteringConstants) == 2 * sizeof(uint32_t));

struct PostBloomDownsampleConstants
{
    uint32_t InputIndex;
    uint32_t OutputIndex;
    uint32_t SamplerIndex;
    uint32_t AdaptedLuminanceIndex;
    uint32_t Threshold;
    uint32_t AutoExposure;
    float    Exposure;
};

struct PostBloomUpsampleConstants
{
    uint32_t DownIndex;
    uint32_t InputIndex;
    uint32_t OutputIndex;
    uint32_t SamplerIndex;
};

// Mirrors the RootConstant0 block that every PostLuminance.slang entry point shares.
struct PostLuminanceConstants
{
    uint32_t InputIndex;
    uint32_t OutputIndex;
    uint32_t PreviousIndex = 0;
    float    AdaptationTime = 0.0f;
};
static_assert(sizeof(PostLuminanceConstants) == 3 * sizeof(uint32_t) + sizeof(float));

struct PostToneConstants
{
    uint32_t SceneColorIndex;
    uint32_t AdaptedLuminanceIndex;
    uint32_t BloomIndex;
    uint32_t SamplerIndex;
    uint32_t ToneMapping;
    float    Exposure;
    float    BloomStrength;
    float    MaxWhite;
    float    P;
    float    A;
    float    M;
    float    L;
    float    C;
    float    B;
    float    StartCompression;
    float    Desaturation;
    uint32_t OutputMode;
    uint32_t EncodeSRGB;
    float    PaperWhiteNits;
    float    PeakNits;
};

static uint32_t BloomLevelSize(uint32_t Size, uint32_t Level)
{
    const uint32_t Shifted = Size >> Level;
    return Shifted ? Shifted : 1;
}

static hz::CVar GExposure("r.Exposure", 0.95f, 0.0f, "Exposure compensation multiplier.");
static hz::CVar GBloom("r.Bloom", true, "Bloom post effect.");
static hz::CVar GBloomStrength("r.Bloom.Strength", 0.01f, 0.0f, "Bloom contribution to the final color.");
static hz::CVar GAdaptationSpeed("r.Exposure.AdaptationSpeed", 3.0f, 0.0f, "Luminance adaptation rate.");

PostEffects::PostEffects(hz::RenderContext& Context): Context(Context)
{
    Sampler = Context.CreateSampler(
        { .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    MeteringPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostMetering.slang" },
        .Name = "Renderer.HDR.Metering",
    });
    ReducePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSReduce" } }, .pFileName = "PostLuminance.slang" },
        .Name = "Renderer.HDR.ReduceLuminance",
    });
    InitializePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSInitialize" } }, .pFileName = "PostLuminance.slang" },
        .Name = "Renderer.HDR.InitializeExposure",
    });
    AdaptationPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSAdapt" } }, .pFileName = "PostLuminance.slang" },
        .Name = "Renderer.HDR.Adaptation",
    });
    BloomDownsamplePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomDownsample.slang" },
        .Name = "Renderer.HDR.BloomDownsample",
    });
    BloomUpsamplePipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "PostBloomUpsample.slang" },
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
            .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                        { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
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
        hz::DependencyList           InitializeDependencies;
        const PostLuminanceConstants Initialize = { .InputIndex = InitializeDependencies.Store(Targets.PreviousAdapted),
                                                    .OutputIndex = InitializeDependencies.Store(Targets.Adapted) };
        Commands.SetPipeline(InitializePipeline);
        Commands.SetRootConstants(Initialize);
        Commands.Dispatch(1, 1, 1, InitializeDependencies.Get());
    }
    constexpr uint32_t          MeteringSize = PostEffectsTargets::MeteringSize;
    hz::DependencyList          MeteringDependencies;
    const PostMeteringConstants Metering = { .SceneColorIndex = MeteringDependencies.Sample(&SceneColor),
                                             .LuminanceOutputIndex = MeteringDependencies.Store(Targets.Luminance[0]) };
    Commands.SetPipeline(MeteringPipeline);
    Commands.SetRootConstants(Metering);
    Commands.Dispatch(MeteringSize / 16, MeteringSize / 16, 1, MeteringDependencies.Get());
    Commands.SetPipeline(ReducePipeline);
    for (uint32_t Level = 1; Level < PostEffectsTargets::LuminanceLevels; ++Level)
    {
        const uint32_t               Size = MeteringSize >> Level;
        hz::DependencyList           ReduceDependencies;
        const PostLuminanceConstants Reduce = { .InputIndex = ReduceDependencies.Sample(Targets.Luminance[Level - 1]),
                                                .OutputIndex = ReduceDependencies.Store(Targets.Luminance[Level]) };
        Commands.SetRootConstants(Reduce);
        Commands.Dispatch((Size + 15) / 16, (Size + 15) / 16, 1, ReduceDependencies.Get());
    }
    RunBloom(Commands, SceneColor, Targets);
    RunAdaptation(Commands, DeltaSeconds, Targets);
    // Match the reference: bloom and tone mapping use last frame's exposure; this frame's is used next frame.
    ToneMap(Commands, SceneColor, RenderTarget, Targets);
    Commands.EndGpuTimestamp();
}

void PostEffects::RunBloom(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const PostEffectsTargets& Targets)
{
    if (!GBloom.GetBool() || !Targets.BloomDown[0])
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
        .InputIndex = SceneColor.GetSrvIndex(),
        .OutputIndex = BloomDown[0]->GetUavIndex(),
        .SamplerIndex = Sampler.GetIndex(),
        .AdaptedLuminanceIndex = Previous.GetSrvIndex(),
        .Threshold = 1,
        .AutoExposure = Settings.ToneMapping != ToneMappingMode::None ? 1u : 0u,
        .Exposure = GExposure.GetFloat(),
    };
    Commands.SetRootConstants(Downsample);
    Commands.Dispatch((BloomWidth + 15) / 16, (BloomHeight + 15) / 16, 1,
                      { .sampledTextures = { &SceneColor, &Previous }, .storageTextures = { BloomDown[0] } });
    // Only the scene downsample is thresholded; the mip levels already carry bright values.
    Downsample.Threshold = 0;
    for (uint32_t Level = 1; Level < BloomLevels; ++Level)
    {
        Downsample.InputIndex = BloomDown[Level - 1]->GetSrvIndex();
        Downsample.OutputIndex = BloomDown[Level]->GetUavIndex();
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
        const PostBloomUpsampleConstants Upsample = { .DownIndex = BloomDown[Level]->GetSrvIndex(),
                                                      .InputIndex = Input->GetSrvIndex(),
                                                      .OutputIndex = BloomUp[Level]->GetUavIndex(),
                                                      .SamplerIndex = Sampler.GetIndex() };
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
        .InputIndex = Dependencies.Sample(Targets.Luminance[PostEffectsTargets::LuminanceLevels - 1]),
        .OutputIndex = Dependencies.Store(Targets.Adapted),
        .PreviousIndex = Dependencies.Sample(Targets.PreviousAdapted),
        .AdaptationTime = fmaxf(DeltaSeconds, 0.0f) * fmaxf(GAdaptationSpeed.GetFloat(), 0.0f),
    };
    Commands.SetPipeline(AdaptationPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch(1, 1, 1, Dependencies.Get());
}

void PostEffects::ToneMap(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget,
                          const PostEffectsTargets& Targets)
{
    const uint32_t     Width = RenderTarget.GetWidth();
    const uint32_t     Height = RenderTarget.GetHeight();
    const bool SrgbTarget = Context.GetColorFormat() == hz::Format::R8G8B8A8_SRGB || Context.GetColorFormat() == hz::Format::B8G8R8A8_SRGB;
    hz::DependencyList Dependencies;
    const hz::GPUTexture*   Bloom = GBloom.GetBool() ? Targets.BloomUp[0] : nullptr;
    const PostToneConstants Constants = {
        .SceneColorIndex = Dependencies.Sample(&SceneColor),
        .AdaptedLuminanceIndex = Dependencies.Sample(Targets.PreviousAdapted),
        .BloomIndex = Dependencies.Sample(Bloom),
        .SamplerIndex = Sampler.GetIndex(),
        .ToneMapping = (uint32_t)Settings.ToneMapping,
        .Exposure = GExposure.GetFloat(),
        .BloomStrength = GBloom.GetBool() && Bloom ? GBloomStrength.GetFloat() : 0.0f,
        .MaxWhite = Settings.MaxWhite,
        .P = Settings.UchimuraP,
        .A = Settings.UchimuraA,
        .M = Settings.UchimuraM,
        .L = Settings.UchimuraL,
        .C = Settings.UchimuraC,
        .B = Settings.UchimuraB,
        .StartCompression = Settings.StartCompression,
        .Desaturation = Settings.Desaturation,
        .OutputMode = (uint32_t)Context.GetOutputMode(),
        .EncodeSRGB = Context.GetOutputMode() == hz::OUTPUT_MODE_SDR && !SrgbTarget ? 1u : 0u,
        .PaperWhiteNits = fmaxf(Settings.PaperWhiteNits, 1.0f),
        .PeakNits = Context.IsHDREnabled() ? Context.GetHDRMetadata().maxContentLightLevel : 100.0f,
    };
    Commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &RenderTarget, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        Dependencies.Get());
    Commands.SetViewport(0, 0, (float)Width, (float)Height);
    Commands.SetScissor(0, 0, Width, Height);
    Commands.SetPipeline(ToneMapPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Draw(3);
    Commands.EndRendering();
}
