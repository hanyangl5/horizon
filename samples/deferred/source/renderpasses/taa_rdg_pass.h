#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class TAARDGPass : public Horizon::Backend::RDGPass
{
  public:
    struct TAAPrevCurrOffset
    {
        Math::float2 prev_offset;
        Math::float2 curr_offset;
    };
  public:
    TAARDGPass(RHI *rhi);
    ~TAARDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle previous_color, Horizon::Backend::TextureHandle pp_color,
                         Horizon::Backend::TextureHandle gbuffer4);
    const Math::float2 &GetJitterOffset() noexcept;
    Buffer *GetTAAPrevCurrOffsetBuffer() const { return m_taa_prev_curr_offset_buffer; }
    void UpdateTAAPrevCurrOffset(const void *data, u32 size);
    Horizon::Backend::TextureHandle GetOutputColorHandle() const { return m_output_color_handle; }
    Horizon::Backend::TextureHandle GetPreviousColorHandle() const { return m_previous_color_handle; }

  private:
    RHI *m_rhi;

    Shader *m_taa_cs;
    Pipeline *m_taa_pipeline;

    Texture *m_previous_color_texture;
    Texture *m_output_color_texture;

    static constexpr u32 TAA_SAMPLE_COUNT = 16;
    std::array<Math::float2, TAA_SAMPLE_COUNT> m_taa_samples;
    u32 m_taa_sample_index = 0;

    TAAPrevCurrOffset m_taa_prev_curr_offset;
    Buffer *m_taa_prev_curr_offset_buffer;

    Horizon::Backend::TextureHandle m_previous_color_handle;
    Horizon::Backend::TextureHandle m_pp_color_handle;
    Horizon::Backend::TextureHandle m_gbuffer4_handle;
    Horizon::Backend::TextureHandle m_output_color_handle;
};
