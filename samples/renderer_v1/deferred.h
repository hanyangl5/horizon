#pragma once

#include "header.h"

class DeferredData {
  public:
    explicit DeferredData(RHI *rhi) noexcept;
    ~DeferredData() noexcept;

    Backend::RHI *mRhi;

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
    Buffer *deferred_shading_constants_buffer;

    RenderTarget *gbuffer0;
    RenderTarget *gbuffer1;
    RenderTarget *gbuffer2;
    RenderTarget *gbuffer3;
    RenderTarget *gbuffer4;
    //RenderTarget* vbuffer0;

    RenderTarget *depth;

    Texture *shading_color_image;

    // ibl
    struct DiffuseIrradianceSH3 {
        std::array<Math::float4, 9> sh;
    } diffuse_irradiance_sh3_constants;
    Buffer *diffuse_irradiance_sh3_buffer;
    TextureDataDesc prefilered_irradiance_env_map_data;
    Texture *prefiltered_irradiance_env_map;
    TextureDataDesc brdf_lut_data_desc;
    Texture *brdf_lut;

    Sampler *ibl_sampler{};
};