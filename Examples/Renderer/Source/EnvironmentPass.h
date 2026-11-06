#pragma once

#include "Graphics/RenderContext.h"

struct EnvironmentFiles
{
    ResourceDirectory directory = RD_TEXTURES;
    const char*       prefiltered = "immenstadter_horn_2k_prefilter.ktx";
    const char*       irradiance = "immenstadter_horn_2k_irradiance.ktx";
    const char*       brdfLut = "brdfLUT.ktx";
};

class EnvironmentPass
{
public:
    EnvironmentPass(hz::RenderContext& context, hz::Format colorFormat = hz::Format::R32G32B32A32_SFLOAT,
                    hz::Format depthFormat = hz::Format::D24_UNORM_S8_UINT);
    // Resource replacement and unload require completion of earlier draws using this environment.
    bool load(const EnvironmentFiles& files = {});
    bool isValid() const;
    void execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& color, const hz::GPUTexture& depth,
                 float intensity = 1.0f) const;

    const hz::GPUTexture& getPrefiltered() const { return prefiltered; }
    const hz::GPUTexture& getIrradiance() const { return irradiance; }
    const hz::GPUTexture& getBrdfLut() const { return brdfLut; }
    const hz::GPUSampler& getSampler() const { return sampler; }
    uint32_t              getPrefilteredMipCount() const { return prefilteredMipCount; }

private:
    hz::RenderContext& context;
    hz::GPUPipeline    pipeline;
    hz::GPUSampler     sampler;
    hz::GPUTexture     prefiltered;
    hz::GPUTexture     irradiance;
    hz::GPUTexture     brdfLut;
    uint32_t           prefilteredMipCount = 0;
};
