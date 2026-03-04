#pragma once
#include "../header.h"
#include "../scene.h"
#include "taa_rdg_pass.h"
#include <render_graph/frame_graph.h>

class DeferredShadingRDGPass;
class DeferredShadingGeometryPass;
class GTAORDGPass;
class PostProcessRDGPass;
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
                            Horizon::Backend::TextureHandle gtao_factor, Horizon::Backend::TextureHandle gtao_blur,
                            Horizon::Backend::TextureHandle output_color,
                            Horizon::Backend::TextureHandle previous_color, Horizon::Backend::TextureHandle brdf_lut,
                            Horizon::Backend::TextureHandle prefiltered_env);
    void SetPassPointers(DeferredShadingGeometryPass *geometry, DeferredShadingRDGPass *deferred, GTAORDGPass *gtao,
                         PostProcessRDGPass *post_process, TAARDGPass *taa);
    void SetFirstFrame(bool first_frame)
    {
        m_first_frame = first_frame;
    }
    void SetUploadSceneResources(bool upload_scene_resources)
    {
        m_upload_scene_resources = upload_scene_resources;
    }
    void SetInitializeHistory(bool initialize_history)
    {
        m_initialize_history = initialize_history;
    }
    void SetTAAPrevCurrOffset(const TAARDGPass::TAAPrevCurrOffset &offset)
    {
        m_taa_prev_curr_offset = offset;
    }

  private:
    [[maybe_unused]] RHI *m_rhi;
    Horizon::SceneManager *m_scene_manager;

    DeferredShadingGeometryPass *m_geometry{nullptr};
    DeferredShadingRDGPass *m_deferred{nullptr};
    GTAORDGPass *m_gtao{nullptr};
    PostProcessRDGPass *m_post_process{nullptr};
    TAARDGPass *m_taa{nullptr};

    bool m_first_frame = false;
    bool m_upload_scene_resources = false;
    bool m_initialize_history = false;
    TAARDGPass::TAAPrevCurrOffset m_taa_prev_curr_offset{};

    Horizon::Backend::TextureHandle m_shading_color_handle;
    Horizon::Backend::TextureHandle m_pp_color_handle;
    Horizon::Backend::TextureHandle m_gtao_factor_handle;
    Horizon::Backend::TextureHandle m_gtao_blur_handle;
    Horizon::Backend::TextureHandle m_output_color_handle;
    Horizon::Backend::TextureHandle m_previous_color_handle;
    Horizon::Backend::TextureHandle m_brdf_lut_handle;
    Horizon::Backend::TextureHandle m_prefiltered_env_handle;
};
