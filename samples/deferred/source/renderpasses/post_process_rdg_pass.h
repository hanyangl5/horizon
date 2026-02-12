#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class PostProcessRDGPass : public Horizon::Backend::RDGPass
{
  public:
    struct ExposureConstant
    {
        Math::float4 exposure_ev100__;
    };

  public:
    PostProcessRDGPass(RHI *rhi, u32 width, u32 height);
    ~PostProcessRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle shading_color,
                         Horizon::Backend::BufferHandle adapted_luminance);
    Buffer *GetConstantsBuffer() const
    {
        return m_exposure_constants_buffer;
    }
    void UpdateConstants(const void *data, u32 size);
    ExposureConstant &GetExposureConstants()
    {
        return m_exposure_constants;
    }
    Horizon::Backend::TextureHandle GetPPColorHandle() const
    {
        return m_pp_color_handle;
    }

  private:
    RHI *m_rhi;
    u32 m_width;
    u32 m_height;

    Shader *m_post_process_cs;
    Pipeline *m_post_process_pipeline;
    ExposureConstant m_exposure_constants;
    Buffer *m_exposure_constants_buffer;

    Texture *m_pp_color_image;

    Horizon::Backend::TextureHandle m_shading_color_handle;
    Horizon::Backend::TextureHandle m_pp_color_handle;
    Horizon::Backend::BufferHandle m_adapted_luminance_handle;
};
