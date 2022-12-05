
#pragma once
#include "config.h"
#include "deferred.h"
#include "post_process.h"
#include "scene.h"
#include "ssao.h"
#include "antialiasing.h"

// HorizonPipeline

class HorizonPipeline {
  public:
    HorizonPipeline() {
        HorizonConfig config{};
        config.width = _width;
        config.height = _height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<HorizonRuntime>(config);
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
    std::unique_ptr<HorizonRuntime> engine{};
    std::filesystem::path asset_path = "C:/FILES/horizon/horizon/assets";

    Horizon::Backend::RHI *rhi{};
    SwapChain *swap_chain{};

    // pass resources

    Sampler *sampler;

    std::unique_ptr<DeferredData> deferred{};
    std::unique_ptr<SSAOData> ssao{};
    std::unique_ptr<PostProcessData> post_process{};
    std::unique_ptr<AntialiasingData> antialiasing{};
    std::unique_ptr<SceneData> scene{};
};
