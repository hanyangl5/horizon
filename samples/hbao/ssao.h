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

    static constexpr u32 HBAO_RAND_TEX_WIDTH = 4;
    static constexpr u32 HBAO_RAND_TEX_HEIGHT = 4;
    static constexpr u32 HBAO_RAND_SIZE = HBAO_RAND_TEX_WIDTH * HBAO_RAND_TEX_HEIGHT;
    static constexpr u32 HBAO_MAX_SAMPLES = 8;

    enum AO_METHOD { SSAO, HBAO, HDAO, GTAO };

    struct AOConstant {
        Math::float4x4 proj;
        Math::float4x4 inv_proj;
        Math::float4x4 view;
        u32 width;
        u32 height;
        f32 noise_scale_x;
        f32 noise_scale_y;
        f32 g_RadiusToScreen;
        f32 g_NDotVBias;
        f32 g_NegInvR2;
        u32 ao_method;
        std::array<Math::float4, SSAO_KERNEL_SIZE> kernels;
    } ao_constansts;

    static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
    static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;
    std::array<Math::float2, SSAO_NOISE_TEX_WIDTH * SSAO_NOISE_TEX_HEIGHT> ssao_noise_tex_val;
    TextureDataDesc ssao_noise_tex_data_desc{};
    Texture *ssao_noise_tex;

    Texture *ssao_factor_image;
    Texture *ssao_blur_image;

    Buffer *ssao_constants_buffer;

    TextureDataDesc hbao_noise_tex_data_desc{};
    Texture *hbao_noise_tex;
};
