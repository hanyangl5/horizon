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

class PostEffects
{
public:
    explicit PostEffects(hz::RenderContext& Context);
    void Resize(uint32_t Width, uint32_t Height);
    void Execute(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget, float DeltaSeconds);
    void ResetExposure() { ExposureInitialized = false; }

    const hz::GPUTexture*     GetBloom() const { return BloomUp[0]; }
    const hz::GPUTexture&     GetAdaptedLuminance() const { return AdaptedLuminance[AdaptedIndex]; }
    const hz::GPUTexture&     GetLuminance(uint32_t Level) const { return Luminance[Level]; }
    static constexpr uint32_t MeteringSize = 512;
    static constexpr uint32_t LuminanceLevels = 10;
    static constexpr uint32_t BloomLevels = 6;
    PostEffectsSettings       Settings;

private:
    void RunBloom(hz::CommandList& Commands, const hz::GPUTexture& SceneColor);
    void RunAdaptation(hz::CommandList& Commands, float DeltaSeconds);
    void ToneMap(hz::CommandList& Commands, const hz::GPUTexture& SceneColor, const hz::GPUTexture& RenderTarget);
    bool PrepareBloomChain();
    void ReleaseBloomChain();

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
    // Pooled per frame; resize creates the chain so execute only borrows it.
    hz::GPUTexture* BloomDown[BloomLevels] = {};
    hz::GPUTexture* BloomUp[BloomLevels - 1] = {};
    hz::FixedArray<hz::GPUTexture, 2>               AdaptedLuminance;
    hz::FixedArray<hz::GPUTexture, LuminanceLevels> Luminance;
    uint32_t                                        Width = 0;
    uint32_t                                        Height = 0;
    uint32_t                                        BloomWidth = 0;
    uint32_t                                        BloomHeight = 0;
    uint32_t                                        AdaptedIndex = 0;
    bool                                            ExposureInitialized = false;
};
