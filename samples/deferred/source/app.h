
#pragma once

#include "renderpasses/deferred_geometry_rdg_pass.h"
#include "renderpasses/deferred_shading_rdg_pass.h"
#include "renderpasses/luminance_average_rdg_pass.h"
#include "renderpasses/luminance_histogram_rdg_pass.h"
#include "renderpasses/post_process_rdg_pass.h"
#include "renderpasses/resource_upload_pass.h"
#include "renderpasses/ssao_blur_rdg_pass.h"
#include "renderpasses/ssao_rdg_pass.h"
#include "renderpasses/taa_rdg_pass.h"
#include "scene.h"
#include <app_framework/app_framework.h>
#include <render_graph/frame_graph.h>

extern Horizon::Path shader_dir;
extern Horizon::Path asset_path;

class DeferredRenderApp : public Horizon::AppFramework
{
  public:
    DeferredRenderApp();

  protected:
    void Initialize() override;
    void RenderLoop() override;
    void Cleanup() override;
    void OnResize(u32 new_width, u32 new_height) override;

  private:
    void InitAPI();
    void InitResources();
    void InitPipelineResources(); // create pass related resource, shader, pipeline, buffer/tex/rt
    void ResizePipelineResources(u32 new_width, u32 new_height);
    void UpdatePipelineResources();

    Horizon::Backend::RHI *rhi{};
    SwapChain *swap_chain{};
    std::unique_ptr<Horizon::Backend::FrameGraph> frame_graph{};

    // pass resources
    Sampler *sampler{};

    // RDG Passes
    std::unique_ptr<DeferredShadingGeometryPass> geometry_pass{};
    std::unique_ptr<DeferredShadingRDGPass> deferred_shading_pass{};
    std::unique_ptr<SSAORDGPass> ssao_pass{};
    std::unique_ptr<SSAOBlurRDGPass> ssao_blur_pass{};
    std::unique_ptr<PostProcessRDGPass> post_process_pass{};
    std::unique_ptr<LuminanceHistogramRDGPass> luminance_histogram_pass{};
    std::unique_ptr<LuminanceAverageRDGPass> luminance_average_pass{};
    std::unique_ptr<TAARDGPass> taa_pass{};
    std::unique_ptr<ResourceUploadRDGPass> resource_upload_pass{};

    std::unique_ptr<SceneData> scene{};
    bool m_first_frame{true};
    bool m_reset_history{false};
    bool m_upload_scene_resources{true};
    u32 m_width{};
    u32 m_height{};
    TAARDGPass::TAAPrevCurrOffset m_taa_prev_curr_offset{};
};
