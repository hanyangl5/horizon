
#pragma once

#include "deferred.h"
#include "header.h"
#include "scene.h"
#include "ssao.h"

// HorizonPipeline

class HorizonPipeline {
  public:
    HorizonPipeline() {
        HorizonConfig config{};
        config.width = width;
        config.height = height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<HorizonRuntime>(config);
    }
    ~HorizonPipeline() {
        rhi->DestroySampler(sampler);
        rhi->DestroySwapChain(swap_chain);

        deferred = nullptr;
        ssao = nullptr;
        scene = nullptr;

        engine = nullptr;
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

    Horizon::Backend::RHI *rhi{};
    SwapChain *swap_chain{};

    // pass resources

    Sampler *sampler;

    std::unique_ptr<DeferredData> deferred{};
    std::unique_ptr<SSAOData> ssao{};
    std::unique_ptr<SceneData> scene{};
};
