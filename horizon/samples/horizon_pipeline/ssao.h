#pragma once

#include "config.h"

class SSAOData {
  public:
    SSAOData(Backend::RHI *rhi) noexcept;
    SSAOData() noexcept;
    // pass resources
    Shader *ssao_cs;

    Pipeline *ssao_pass;

    Shader *ssao_blur_cs;
    Pipeline *ssao_blur_pass;

    // 
    static constexpr u32 SSAO_KERNEL_SIZE = 32;

    struct SSAOConstant {
        math::Matrix44f inv_proj;
        f32 noise_scale_x;
        f32 noise_scale_y;
        u32 pad0[2];
        Container::FixedArray<math::Vector4f, SSAO_KERNEL_SIZE> kernels;
    } ssao_constansts;

    static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
    static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;
    Container::FixedArray<math::Vector2f, SSAO_NOISE_TEX_WIDTH * SSAO_NOISE_TEX_HEIGHT> ssao_noise_tex_val;
    TextureDataDesc ssao_noise_tex_data_desc{};
    Texture* ssao_noise_tex;

    Texture* ssao_factor_texture;
    Texture* ssao_blur_texture;

    Buffer* ssao_constants_buffer;
};
