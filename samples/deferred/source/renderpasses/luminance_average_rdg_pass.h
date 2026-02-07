#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class LuminanceHistogramRDGPass;

class LuminanceAverageRDGPass : public Horizon::Backend::RDGPass
{
  public:
    LuminanceAverageRDGPass(RHI *rhi);
    ~LuminanceAverageRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::BufferHandle histogram_buffer, Horizon::Backend::BufferHandle adapted_luminance);
    void SetLuminanceHistogramPass(LuminanceHistogramRDGPass *luminance_histogram_pass);

  private:
    [[maybe_unused]] RHI *m_rhi;

    Shader *m_luminance_average_cs;
    Pipeline *m_luminance_average_pipeline;

    LuminanceHistogramRDGPass *m_luminance_histogram_pass{nullptr};

    Horizon::Backend::BufferHandle m_histogram_buffer_handle;
    Horizon::Backend::BufferHandle m_adapted_luminance_handle;
};
