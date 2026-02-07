#pragma once
#include "../header.h"
#include "../scene.h"
#include <render_graph/frame_graph.h>
#include "taa_rdg_pass.h"

class DeferredShadingRDGPass;
class SSAORDGPass;
class PostProcessRDGPass;
class LuminanceHistogramRDGPass;
class TAARDGPass;

class ResourceUploadRDGPass : public Horizon::Backend::RDGPass
{
  public:
    ResourceUploadRDGPass(RHI *rhi, Horizon::SceneManager *scene_manager);
    ~ResourceUploadRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetResourceHandles(Horizon::Backend::TextureHandle shading_color, Horizon::Backend::TextureHandle pp_color,
                             Horizon::Backend::TextureHandle ssao_factor, Horizon::Backend::TextureHandle ssao_blur,
                             Horizon::Backend::TextureHandle output_color, Horizon::Backend::TextureHandle previous_color,
                             Horizon::Backend::TextureHandle ssao_noise, Horizon::Backend::TextureHandle brdf_lut,
                             Horizon::Backend::TextureHandle prefiltered_env, Horizon::Backend::BufferHandle histogram_buffer,
                             Horizon::Backend::BufferHandle adapted_luminance);
    void SetPassPointers(DeferredShadingRDGPass *deferred, SSAORDGPass *ssao, PostProcessRDGPass *post_process,
                         LuminanceHistogramRDGPass *luminance_histogram, TAARDGPass *taa);
    void SetFirstFrame(bool first_frame) { m_first_frame = first_frame; }
    void SetTAAPrevCurrOffset(const TAARDGPass::TAAPrevCurrOffset &offset) { m_taa_prev_curr_offset = offset; }

  private:
    RHI *m_rhi;
    Horizon::SceneManager *m_scene_manager;

    DeferredShadingRDGPass *m_deferred{nullptr};
    SSAORDGPass *m_ssao{nullptr};
    PostProcessRDGPass *m_post_process{nullptr};
    LuminanceHistogramRDGPass *m_luminance_histogram{nullptr};
    TAARDGPass *m_taa{nullptr};

    bool m_first_frame = false;
    TAARDGPass::TAAPrevCurrOffset m_taa_prev_curr_offset{};

    Horizon::Backend::TextureHandle m_shading_color_handle;
    Horizon::Backend::TextureHandle m_pp_color_handle;
    Horizon::Backend::TextureHandle m_ssao_factor_handle;
    Horizon::Backend::TextureHandle m_ssao_blur_handle;
    Horizon::Backend::TextureHandle m_output_color_handle;
    Horizon::Backend::TextureHandle m_previous_color_handle;
    Horizon::Backend::TextureHandle m_ssao_noise_handle;
    Horizon::Backend::TextureHandle m_brdf_lut_handle;
    Horizon::Backend::TextureHandle m_prefiltered_env_handle;
    Horizon::Backend::BufferHandle m_histogram_buffer_handle;
    Horizon::Backend::BufferHandle m_adapted_luminance_handle;
};
