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
    uint32_t        bloomPasses = 2;
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

    const hz::GPUTexture&     getBrightPass() const { return brightPass; }
    const hz::GPUTexture&     getBloom() const { return bloomPass; }
    const hz::GPUTexture&     getAdaptedLuminance() const { return adaptedLuminance[adaptedIndex]; }
    const hz::GPUTexture&     getLuminance(uint32_t level) const { return luminance[level]; }
    static constexpr uint32_t bloomSize = 512;
    static constexpr uint32_t luminanceLevels = 10;
    PostEffectsSettings       settings;

private:
    void runBloom(hz::CommandList& commands);
    void runAdaptation(hz::CommandList& commands, float deltaSeconds);
    void toneMap(hz::CommandList& commands, const hz::GPUTexture& sceneColor, const hz::GPUTexture& renderTarget);

    hz::RenderContext&                              context;
    hz::GPUSampler                                  sampler;
    hz::GPUPipeline                                 brightPipeline;
    hz::GPUPipeline                                 reducePipeline;
    hz::GPUPipeline                                 initializePipeline;
    hz::GPUPipeline                                 adaptationPipeline;
    hz::GPUPipeline                                 bloomPipeline;
    hz::GPUPipeline                                 toneMapPipeline;
    hz::Format                                      colorFormat = {};
    hz::GPUTexture                                  brightPass;
    hz::GPUTexture                                  bloomPass;
    hz::FixedArray<hz::GPUTexture, 2>               bloom;
    hz::FixedArray<hz::GPUTexture, 2>               adaptedLuminance;
    hz::FixedArray<hz::GPUTexture, luminanceLevels> luminance;
    uint32_t                                        width = 0;
    uint32_t                                        height = 0;
    uint32_t                                        adaptedIndex = 0;
    bool                                            exposureInitialized = false;
};
