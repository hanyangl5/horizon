#pragma once

#include "header.h"

class AmbientOcclusionPass
{
  public:
    explicit AmbientOcclusionPass(Backend::RHI *rhi) noexcept;
    ~AmbientOcclusionPass() noexcept;

    RHI *mRhi;

    // pass resources
    Shader *ssao_cs;

    Pipeline *ssao_pass;

    Shader *ssao_blur_cs;
    Pipeline *ssao_blur_pass;

    //
    static constexpr u32 SSAO_KERNEL_SIZE = 32;

    struct SSAOConstant
    {
        Math::float4x4 proj;
        Math::float4x4 inv_proj;
        Math::float4x4 view;
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
    Texture *ssao_noise_tex;

    Texture *ssao_factor_image;
    Texture *ssao_blur_image;

    Buffer *ssao_constants_buffer;
};

// class AmbientOcclusionRDGPass : public RDGPass
// {
//   public:
//     AmbientOcclusionRDGPass(const RDG& rdg) : RDGPass("AmbientOcclusion", rdg) {}
//     ~AmbientOcclusionRDGPass() noexcept {}
//     void Setup() override;
//     void Execute(CommandList *command_list) override;
// };

// class AmbientOcclusionBlurRDGPass : public RDGPass
// {
//   public:
//     AmbientOcclusionBlurRDGPass(const RDG& rdg) : RDGPass("AmbientOcclusionBlur", rdg) {}
//     ~AmbientOcclusionBlurRDGPass() noexcept {}
//     void Setup() override;
//     void Execute(CommandList *command_list) override;
// };