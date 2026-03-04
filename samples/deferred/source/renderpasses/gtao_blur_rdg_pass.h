#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class GTAOBlurRDGPass : public Horizon::Backend::RDGPass
{
  public:
    GTAOBlurRDGPass(Horizon::Backend::RHI *rhi, u32 width, u32 height);
    ~GTAOBlurRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(Horizon::CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle gtao_factor, Horizon::Backend::TextureHandle depth,
                         Horizon::Backend::TextureHandle gbuffer0);
    Horizon::Backend::TextureHandle GetOutputHandle() const
    {
        return m_gtao_blur_handle;
    }

  private:
    Horizon::Backend::RHI *m_rhi;
    u32 m_width;
    u32 m_height;

    Shader *m_gtao_blur_cs;
    Pipeline *m_gtao_blur_pipeline;

    Texture *m_gtao_blur_image;

    Horizon::Backend::TextureHandle m_gtao_factor_handle;
    Horizon::Backend::TextureHandle m_depth_handle;
    Horizon::Backend::TextureHandle m_gbuffer0_handle;
    Horizon::Backend::TextureHandle m_gtao_blur_handle;
};
