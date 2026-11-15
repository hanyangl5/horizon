#pragma once

#include "Graphics/RenderContext.h"

struct EnvironmentFiles
{
    ResourceDirectory Directory = RD_TEXTURES;
    const char*       Prefiltered = "immenstadter_horn_2k_prefilter.ktx";
    const char*       Irradiance = "immenstadter_horn_2k_irradiance.ktx";
    const char*       BrdfLut = "brdfLUT.ktx";
};

class EnvironmentPass
{
public:
    EnvironmentPass(hz::RenderContext& Context, hz::Format ColorFormat = hz::Format::R32G32B32A32_SFLOAT,
                    hz::Format DepthFormat = hz::Format::D24_UNORM_S8_UINT);
    // Resource replacement and unload require completion of earlier draws using this environment.
    bool Load(const EnvironmentFiles& Files = {});
    bool IsValid() const;
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Color, const hz::GPUTexture& Depth,
                 float Intensity = 1.0f) const;

    const hz::GPUTexture& GetPrefiltered() const { return Prefiltered; }
    const hz::GPUTexture& GetIrradiance() const { return Irradiance; }
    const hz::GPUTexture& GetBrdfLut() const { return BrdfLut; }
    const hz::GPUSampler& GetSampler() const { return Sampler; }
    uint32_t              GetPrefilteredMipCount() const { return PrefilteredMipCount; }

private:
    hz::RenderContext& Context;
    hz::GPUPipeline    Pipeline;
    hz::GPUSampler     Sampler;
    hz::GPUTexture     Prefiltered;
    hz::GPUTexture     Irradiance;
    hz::GPUTexture     BrdfLut;
    uint32_t           PrefilteredMipCount = 0;
};
