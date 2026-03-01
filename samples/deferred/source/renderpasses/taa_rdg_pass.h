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
    struct TAAConstants
    {
        f32 history_valid = 0.0f;
        f32 static_curr_weight = 0.08f;
        f32 velocity_scale = 120.0f;
        f32 velocity_disocclusion_threshold = 0.03f;
    };

  public:
    TAARDGPass(RHI *rhi, u32 width, u32 height);
    ~TAARDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle previous_color, Horizon::Backend::TextureHandle pp_color,
                         Horizon::Backend::TextureHandle gbuffer4);
    const Math::float2 &GetJitterOffset() noexcept;
    Buffer *GetTAAConstantsBuffer() const
    {
        return m_taa_constants_buffer;
    }
    void UpdateTAAPrevCurrOffset(const void *data, u32 size);
    Horizon::Backend::TextureHandle GetOutputColorHandle() const
    {
        return m_output_color_handle;
    }
    Horizon::Backend::TextureHandle GetPreviousColorHandle() const
    {
        return m_previous_color_handle;
    }

  private:
    RHI *m_rhi;
    u32 m_width;
    u32 m_height;

    Shader *m_taa_cs;
    Pipeline *m_taa_pipeline;

    Texture *m_previous_color_texture;
    Texture *m_output_color_texture;
    Sampler *m_history_sampler;

    static constexpr u32 TAA_SAMPLE_COUNT = 16;
    std::array<Math::float2, TAA_SAMPLE_COUNT> m_taa_samples;
    u32 m_taa_sample_index = 0;

    Buffer *m_taa_constants_buffer;

    Horizon::Backend::TextureHandle m_previous_color_handle;
    Horizon::Backend::TextureHandle m_pp_color_handle;
    Horizon::Backend::TextureHandle m_gbuffer4_handle;
    Horizon::Backend::TextureHandle m_output_color_handle;
};
