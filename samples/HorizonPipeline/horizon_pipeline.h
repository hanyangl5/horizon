
#pragma once

#include "antialiasing.h"
#include "deferred.h"
#include "post_process.h"
#include "scene.h"
#include "ssao.h"
#include <runtime/render/Config.h>
#include <runtime/render/Render.h>
// HorizonPipeline

class HorizonPipeline {
  public:
    HorizonPipeline() {
        Horizon::Config config{};
        config.width = width;
        config.height = height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.app_type = Horizon::ApplicationType::GRAPHICS;
        window = std::make_unique<Horizon::Window>("horizon", config.width, config.height);
        
        config.window = window.get();
        renderer = std::make_unique<Horizon::Renderer>(config);
    }
    ~HorizonPipeline() {
        rhi->DestroySampler(sampler);
        rhi->DestroySwapChain(swap_chain);

        deferred = nullptr;
        ssao = nullptr;
        post_process = nullptr;
        antialiasing = nullptr;
        scene = nullptr;
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
    std::unique_ptr<Horizon::Renderer> renderer{};
    std::unique_ptr<Horizon::Window> window;
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
