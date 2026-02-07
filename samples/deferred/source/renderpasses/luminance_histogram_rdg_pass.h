#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class LuminanceHistogramRDGPass : public Horizon::Backend::RDGPass
{
  public:
    struct LuminanceHistogramConstants
    {
        u32 width;
        u32 height;
        u32 pixelCount;
        float maxLuminance;
        float timeCoeff;
    }; 
  public:
    LuminanceHistogramRDGPass(RHI *rhi);
    ~LuminanceHistogramRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandle(Horizon::Backend::TextureHandle shading_color);
    Buffer *GetConstantsBuffer() const { return m_luminance_histogram_constants_buffer; }
    void UpdateConstants(const void *data, u32 size);
    Buffer *GetHistogramBuffer() const { return m_histogram_buffer; }
    Buffer *GetAdaptedLuminanceBuffer() const { return m_adapted_luminance_buffer; }
    LuminanceHistogramConstants &GetLuminanceHistogramConstants() { return m_luminance_histogram_constants; }
    Horizon::Backend::BufferHandle GetHistogramBufferHandle() const { return m_histogram_buffer_handle; }
    Horizon::Backend::BufferHandle GetAdaptedLuminanceHandle() const { return m_adapted_luminance_handle; }

  private:
    RHI *m_rhi;

    Shader *m_luminance_histogram_cs;
    Pipeline *m_luminance_histogram_pipeline;

    LuminanceHistogramConstants m_luminance_histogram_constants;
    Buffer *m_luminance_histogram_constants_buffer;
    Buffer *m_histogram_buffer;
    Buffer *m_adapted_luminance_buffer;

    Horizon::Backend::TextureHandle m_shading_color_handle;
    Horizon::Backend::BufferHandle m_histogram_buffer_handle;
    Horizon::Backend::BufferHandle m_adapted_luminance_handle;
};
