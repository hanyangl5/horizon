#pragma once

#include "config.h"

class ShadingData {
  public:
    ShadingData(RHI* rhi) noexcept;
    ~ShadingData() noexcept;

    Shader *shading_cs;
    Pipeline *shading_pass;

    Texture* shading_color_texture;

    // ibl
    struct DiffuseIrradianceSH3 {
        Container::FixedArray<math::Vector4f, 9> sh;
    } diffuse_irradiance_sh3_constants;
    Buffer* diffuse_irradiance_sh3_buffer;
    TextureDataDesc prefilered_irradiance_env_map_data;
    Texture* prefiltered_irradiance_env_map;
    TextureDataDesc brdf_lut_data_desc;
    Texture* brdf_lut;

    Sampler* ibl_sampler{};

    Buffer *shading_buffer{};

};