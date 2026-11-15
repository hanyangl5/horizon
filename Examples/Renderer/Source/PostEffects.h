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
    ToneMappingMode toneMapping = ToneMappingMode::KhronosPBR;
    float           exposure = 0.95f;
    bool            enableBloom = true;
    float           bloomStrength = 0.01f;
    float           adaptationSpeed = 3.0f;
    float           maxWhite = 1.0f;
    float           uchimuraP = 1.0f;
    float           uchimuraA = 1.05f;
    float           uchimuraM = 0.1f;
    float           uchimuraL = 0.8f;
    float           uchimuraC = 3.0f;
    float           uchimuraB = 0.0f;
    float           startCompression = 0.8f;
    float           desaturation = 0.15f;
    float           paperWhiteNits = 203.0f;
};

class PostEffects
{
public:
    explicit PostEffects(hz::RenderContext& context);
    void resize(uint32_t width, uint32_t height);
    void execute(hz::CommandList& commands, const hz::GPUTexture& sceneColor, const hz::GPUTexture& renderTarget, float deltaSeconds);
    void resetExposure() { exposureInitialized = false; }

    const hz::GPUTexture*     getBloom() const { return bloomUp[0]; }
    const hz::GPUTexture&     getAdaptedLuminance() const { return adaptedLuminance[adaptedIndex]; }
    const hz::GPUTexture&     getLuminance(uint32_t level) const { return luminance[level]; }
    static constexpr uint32_t meteringSize = 512;
    static constexpr uint32_t luminanceLevels = 10;
    static constexpr uint32_t bloomLevels = 6;
    PostEffectsSettings       settings;

private:
    void runBloom(hz::CommandList& commands, const hz::GPUTexture& sceneColor);
    void runAdaptation(hz::CommandList& commands, float deltaSeconds);
    void toneMap(hz::CommandList& commands, const hz::GPUTexture& sceneColor, const hz::GPUTexture& renderTarget);
    bool prepareBloomChain();
    void releaseBloomChain();

    hz::RenderContext&                              context;
    hz::GPUSampler                                  sampler;
    hz::GPUPipeline                                 meteringPipeline;
    hz::GPUPipeline                                 reducePipeline;
    hz::GPUPipeline                                 initializePipeline;
    hz::GPUPipeline                                 adaptationPipeline;
    hz::GPUPipeline                                 bloomDownsamplePipeline;
    hz::GPUPipeline                                 bloomUpsamplePipeline;
    hz::GPUPipeline                                 toneMapPipeline;
    hz::Format                                      colorFormat = {};
    // Pooled per frame; resize creates the chain so execute only borrows it.
    hz::GPUTexture* bloomDown[bloomLevels] = {};
    hz::GPUTexture* bloomUp[bloomLevels - 1] = {};
    hz::FixedArray<hz::GPUTexture, 2>               adaptedLuminance;
    hz::FixedArray<hz::GPUTexture, luminanceLevels> luminance;
    uint32_t                                        width = 0;
    uint32_t                                        height = 0;
    uint32_t                                        bloomWidth = 0;
    uint32_t                                        bloomHeight = 0;
    uint32_t                                        adaptedIndex = 0;
    bool                                            exposureInitialized = false;
};
