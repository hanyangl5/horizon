#pragma once

#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"

enum class ToneMappingMode : uint32_t
{
    None,
    Reinhard,
    Uchimura,
    KhronosPBR,
};

struct PostEffectsSettings
{
    ToneMappingMode ToneMapping = ToneMappingMode::KhronosPBR;
    float           Exposure = 0.95f;
    bool            EnableBloom = true;
    float           BloomStrength = 0.01f;
    float           AdaptationSpeed = 3.0f;
    float           MaxWhite = 1.0f;
    float           UchimuraP = 1.0f;
    float           UchimuraA = 1.05f;
    float           UchimuraM = 0.1f;
    float           UchimuraL = 0.8f;
    float           UchimuraC = 3.0f;
    float           UchimuraB = 0.0f;
    float           StartCompression = 0.8f;
    float           Desaturation = 0.15f;
    float           PaperWhiteNits = 203.0f;
};

struct PostEffectsTargets
{
    static constexpr uint32_t MeteringSize = 512;
    static constexpr uint32_t LuminanceLevels = 10;
    static constexpr uint32_t BloomLevels = 6;

    // Log luminance halving from MeteringSize down to 1x1.
    const hz::GPUTexture* Luminance[LuminanceLevels] = {};
    // Downsamples from half the viewport; Up accumulates back, so the smallest level only needs its downsample.
    const hz::GPUTexture* BloomDown[BloomLevels] = {};
    const hz::GPUTexture* BloomUp[BloomLevels - 1] = {};
    // 1x1 exposure: Previous is last frame's adapted value, Adapted receives this frame's.
    const hz::GPUTexture* PreviousAdapted = nullptr;
    const hz::GPUTexture* Adapted = nullptr;
    // False after a resize or reset; the pass then seeds both images before adapting.
    bool                  AdaptedValid = false;
};

class PostEffects
{
public:
    static constexpr hz::Format LuminanceFormat = hz::Format::R16_SFLOAT;
    static constexpr hz::Format AdaptedFormat = hz::Format::R32_SFLOAT;
    static constexpr hz::Format BloomFormat = hz::Format::R16G16B16A16_SFLOAT;

    explicit PostEffects(hz::RenderContext& Context);
    // Rebuilds the tone map pipeline when the swap chain format changes.
    void Resize();
    void Execute(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget, float DeltaSeconds,
                 const PostEffectsTargets& Targets);

    PostEffectsSettings Settings;

private:
    void RunBloom(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const PostEffectsTargets& Targets);
    void RunAdaptation(hz::CommandList& Commands, float DeltaSeconds, const PostEffectsTargets& Targets);
    void ToneMap(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget,
                 const PostEffectsTargets& Targets);

    hz::RenderContext&                              Context;
    hz::GPUSampler                                  Sampler;
    hz::GPUPipeline                                 MeteringPipeline;
    hz::GPUPipeline                                 ReducePipeline;
    hz::GPUPipeline                                 InitializePipeline;
    hz::GPUPipeline                                 AdaptationPipeline;
    hz::GPUPipeline                                 BloomDownsamplePipeline;
    hz::GPUPipeline                                 BloomUpsamplePipeline;
    hz::GPUPipeline                                 ToneMapPipeline;
    hz::Format                                      ColorFormat = {};
};
