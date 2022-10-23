#pragma once

#include "config.h"

class SSAOData {
  public:
    SSAOData(Backend::RHI *rhi) noexcept;

    // pass resources
    Shader *ssao_cs;

    Pipeline *ssao_pass;

    Shader *ssao_blur_cs;
    Pipeline *ssao_blur_pass;

    // 
    static constexpr u32 SSAO_KERNEL_SIZE = 32;

    struct SSAOConstant {
        Math::float4x4 p;
        Math::float4x4 inv_p;
        Math::float4x4 view_mat;
        u32 width;
        u32 height;
        f32 noise_scale_x;
        f32 noise_scale_y;
        std::array<Math::float4, SSAO_KERNEL_SIZE> kernels;
    } ssao_constansts;

    static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
    static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;
    std::array<Math::float2, SSAO_NOISE_TEX_WIDTH * SSAO_NOISE_TEX_HEIGHT> ssao_noise_tex_val;
    TextureDataDesc ssao_noise_tex_data_desc{};
    Resource<Texture> ssao_noise_tex;

    Resource<Texture> ssao_factor_image;
    Resource<Texture> ssao_blur_image;

    Resource<Buffer> ssao_constants_buffer;
};
