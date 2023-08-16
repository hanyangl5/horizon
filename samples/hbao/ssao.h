#pragma once

#include "header.h"

enum AO_METHOD : u32 { SSAO, HBAO, HDAO, GTAO, COUNT };

class AOData {
  public:
    explicit AOData(Backend::RHI *rhi) noexcept;
    ~AOData() noexcept;

    void SetAoMethod(AO_METHOD method);
    AO_METHOD mAoMethod = AO_METHOD::HBAO;
    RHI *mRhi;

    // pass resources
    Shader *ao_cs;

    Pipeline *ao_pass;

    Shader *ao_blur_cs;
    Pipeline *ao_blur_pass;

    //
    static constexpr u32 SSAO_KERNEL_SIZE = 32;

    static constexpr u32 HBAO_RAND_TEX_WIDTH = 4;
    static constexpr u32 HBAO_RAND_TEX_HEIGHT = 4;
    static constexpr u32 HBAO_RAND_SIZE = HBAO_RAND_TEX_WIDTH * HBAO_RAND_TEX_HEIGHT;
    static constexpr u32 HBAO_MAX_SAMPLES = 8;

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
