#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class SSAORDGPass : public Horizon::Backend::RDGPass
{
  public:
    SSAORDGPass(RHI *rhi, Sampler *sampler);
    ~SSAORDGPass() override = default;

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle depth, Horizon::Backend::TextureHandle gbuffer0);
    Buffer *GetConstantsBuffer() const { return m_ssao_constants_buffer; }
    void UpdateConstants(const void *data, u32 size);

  private:
    RHI *m_rhi;
    Sampler *m_sampler;

    Shader *m_ssao_cs;
    Pipeline *m_ssao_pipeline;

    Texture *m_ssao_noise_tex;
    Texture *m_ssao_factor_image;
    Buffer *m_ssao_constants_buffer;

    Horizon::Backend::TextureHandle m_depth_handle;
    Horizon::Backend::TextureHandle m_gbuffer0_handle;
    Horizon::Backend::TextureHandle m_ssao_factor_handle;
    Horizon::Backend::TextureHandle m_ssao_noise_handle;
};
