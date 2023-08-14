#pragma once

#include "header.h"

class DeferredData {
  public:
    explicit DeferredData(RHI *rhi) noexcept;
    ~DeferredData() noexcept;

    Backend::RHI *m_rhi;

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
    } deferred_shading_constants;
    Buffer *deferred_shading_constants_buffer;

    RenderTarget *gbuffer0;
    //RenderTarget *gbuffer1;
    //RenderTarget *gbuffer2;
    //RenderTarget *gbuffer3;

    RenderTarget *depth;

    Texture *shading_color_image;

    Sampler *ibl_sampler{};
};