#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class SSAOBlurRDGPass : public Horizon::Backend::RDGPass
{
  public:
    SSAOBlurRDGPass(Horizon::Backend::RHI *rhi);
    ~SSAOBlurRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(Horizon::CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandle(Horizon::Backend::TextureHandle ssao_factor);
    Horizon::Backend::TextureHandle GetOutputHandle() const
    {
        return m_ssao_blur_handle;
    }

  private:
    Horizon::Backend::RHI *m_rhi;

    Shader *m_ssao_blur_cs;
    Pipeline *m_ssao_blur_pipeline;

    Texture *m_ssao_blur_image;

    Horizon::Backend::TextureHandle m_ssao_factor_handle;
    Horizon::Backend::TextureHandle m_ssao_blur_handle;
};
