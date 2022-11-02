
#pragma once
#include "config.h"
#include "deferred.h"
#include "post_process.h"
#include "scene.h"
#include "ssao.h"

// HorizonPipeline

class HorizonPipeline {
  public:
    HorizonPipeline() {
        EngineConfig config{};
        config.width = _width;
        config.height = _height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<Engine>(config);
    }

    void Init() {
        InitAPI();
        InitResources();
    }
    void InitAPI();

    void InitResources();

    void InitPipelineResources(); // create pass related resource, shader, pipeline, buffer/tex/rt

    void UpdatePipelineResources();

    void run();

  private:
    std::unique_ptr<Engine> engine{};
    std::filesystem::path asset_path = "C:/FILES/horizon/horizon/assets";

    Camera *m_camera{};
    Horizon::Backend::RHI *rhi;
    SwapChain *swap_chain;

    // pass resources

    Sampler *sampler;

    DeferredData *deferred{};
    SSAOData *ssao{};
    PostProcessData *post_process{};
    SceneData *scene{};
};
