#pragma once

#include "config.h"

class DeferredData {
  public:
    DeferredData(Backend::RHI *rhi) noexcept;
    ~DeferredData() noexcept = default;

    // pass resources

    GraphicsPipelineCreateInfo graphics_pass_ci{};

    Shader *geometry_vs, *geometry_ps;
    Pipeline *geometry_pass;

    Shader *shading_cs;
    Pipeline *shading_pass;

    // buffer/texture/rt resources

    struct DeferredShadingConstants {
        Math::float4x4 inverse_vp;
        Math::float4 camera_pos;
        u32 width;
        u32 height;
        float ibl_intensity, pad1;
    } deferred_shading_constants;
    Resource<Buffer> deferred_shading_constants_buffer;

    Resource<RenderTarget> gbuffer0;
    Resource<RenderTarget> gbuffer1;
    Resource<RenderTarget> gbuffer2;
    Resource<RenderTarget> gbuffer3;

    Resource<RenderTarget> depth;

    Resource<Texture> shading_color_image;

    // ibl
    struct DiffuseIrradianceSH3 {
        std::array<Math::float4, 9> sh;
    } diffuse_irradiance_sh3_constants;
    Resource<Buffer> diffuse_irradiance_sh3_buffer;
    TextureDataDesc prefilered_irradiance_env_map_data;
    Resource<Texture> prefiltered_irradiance_env_map;
    TextureDataDesc brdf_lut_data_desc;
    Resource<Texture> brdf_lut;

    Resource<Sampler> ibl_sampler{};
};