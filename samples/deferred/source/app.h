
#pragma once

#include "renderpasses/deferred_shading_rdg_pass.h"
#include "renderpasses/deferred_geometry_rdg_pass.h"
#include "renderpasses/ssao_rdg_pass.h"
#include "renderpasses/ssao_blur_rdg_pass.h"
#include "renderpasses/post_process_rdg_pass.h"
#include "renderpasses/luminance_histogram_rdg_pass.h"
#include "renderpasses/luminance_average_rdg_pass.h"
#include "renderpasses/taa_rdg_pass.h"
#include "renderpasses/resource_upload_pass.h"
#include "scene.h"
#include <render_graph/frame_graph.h>
#include <scene/scene_renderer/renderer.h>
// Render

extern Horizon::Path shader_dir;
extern Horizon::Path asset_path;

class Render
{
  public:
    Render()
    {
        Horizon::Config config{};
        config.width = width;
        config.height = height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.app_type = Horizon::ApplicationType::GRAPHICS;
        window = std::make_unique<Horizon::Window>("horizon", config.width, config.height);

        config.window = window.get();
        renderer = std::make_unique<Horizon::Renderer>(config);
    }
    ~Render()
    {
        rhi->DestroySampler(sampler);
        rhi->DestroySwapChain(swap_chain);

        geometry_pass = nullptr;
        deferred_shading_pass = nullptr;
        ssao_pass = nullptr;
        ssao_blur_pass = nullptr;
        post_process_pass = nullptr;
        luminance_histogram_pass = nullptr;
        luminance_average_pass = nullptr;
        taa_pass = nullptr;
        resource_upload_pass = nullptr;
        scene = nullptr;
    }
    void Init()
    {
        InitAPI();
        InitResources();
    }
    void InitAPI();

    void InitResources();

    void InitPipelineResources(); // create pass related resource, shader, pipeline, buffer/tex/rt

    void UpdatePipelineResources();

    void run();

  private:
    std::unique_ptr<Horizon::Renderer> renderer{};
    std::unique_ptr<Horizon::Window> window;
    Horizon::Backend::RHI *rhi{};
    SwapChain *swap_chain{};
    std::unique_ptr<Horizon::Backend::FrameGraph> frame_graph{};

    // pass resources

    Sampler *sampler;
    
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
};
