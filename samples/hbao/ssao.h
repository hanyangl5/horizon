#pragma once

#include "header.h"

class SSAOData {
  public:
    explicit SSAOData(Backend::RHI *rhi) noexcept;
    ~SSAOData() noexcept;

    RHI *m_rhi;

    // pass resources
    Shader *ssao_cs;

    Pipeline *ssao_pass;

    Shader *ssao_blur_cs;
    Pipeline *ssao_blur_pass;

    //
    static constexpr u32 SSAO_KERNEL_SIZE = 32;

    struct SSAOConstant {
        Math::float4x4 proj;
        Math::float4x4 inv_proj;
        Math::float4x4 view;
        u32 width;
        u32 height;
        f32 noise_scale_x;
        f32 noise_scale_y;
        Container::FixedArray<Math::float4, SSAO_KERNEL_SIZE> kernels;
    } ssao_constansts;

    static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
    static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;
    Container::FixedArray<Math::float2, SSAO_NOISE_TEX_WIDTH * SSAO_NOISE_TEX_HEIGHT> ssao_noise_tex_val;
    TextureDataDesc ssao_noise_tex_data_desc{};
    Texture *ssao_noise_tex;

    Texture *ssao_factor_image;
    Texture *ssao_blur_image;

    Buffer *ssao_constants_buffer;
};
