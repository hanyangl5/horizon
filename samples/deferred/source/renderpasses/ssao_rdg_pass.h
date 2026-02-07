#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class SSAORDGPass : public Horizon::Backend::RDGPass
{
  public:
    SSAORDGPass(RHI *rhi, Sampler *sampler);
    ~SSAORDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle depth, Horizon::Backend::TextureHandle gbuffer0);
    Buffer *GetConstantsBuffer() const { return m_ssao_constants_buffer; }
    void UpdateConstants(const void *data, u32 size);
    Texture *GetSSAONoiseTex() const { return m_ssao_noise_tex; }
    TextureDataDesc &GetSSAONoiseTexDataDesc() { return m_ssao_noise_tex_data_desc; }
    Horizon::Backend::TextureHandle GetSSAOFactorHandle() const { return m_ssao_factor_handle; }
    Horizon::Backend::TextureHandle GetSSAONoiseHandle() const { return m_ssao_noise_handle; }

    struct SSAOConstant
    {
        Math::float4x4 proj;
        Math::float4x4 inv_proj;
        Math::float4x4 view;
        u32 width;
        u32 height;
        f32 noise_scale_x;
        f32 noise_scale_y;
        std::array<Math::float4, 32> kernels;
    };
    SSAOConstant &GetSSAOConstants() { return m_ssao_constants; }
    static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
    static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;

  private:
    RHI *m_rhi;
    Sampler *m_sampler;

    Shader *m_ssao_cs;
    Pipeline *m_ssao_pipeline;

    Texture *m_ssao_noise_tex;
    Texture *m_ssao_factor_image;
    Buffer *m_ssao_constants_buffer;
    TextureDataDesc m_ssao_noise_tex_data_desc{};
    SSAOConstant m_ssao_constants{};

    Horizon::Backend::TextureHandle m_depth_handle;
    Horizon::Backend::TextureHandle m_gbuffer0_handle;
    Horizon::Backend::TextureHandle m_ssao_factor_handle;
    Horizon::Backend::TextureHandle m_ssao_noise_handle;
};
