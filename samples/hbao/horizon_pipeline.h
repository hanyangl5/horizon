
#pragma once

#include "deferred.h"
#include "header.h"
#include "scene.h"
#include "ssao.h"

#include <runtime/render/Render.h>

class HorizonPipeline {
  public:
    HorizonPipeline() {
        Config config{};
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

    void Run();

  private:
    std::unique_ptr<Renderer> renderer{};
    std::unique_ptr<Window> window;
    Horizon::Backend::RHI *rhi{};
    SwapChain *swap_chain{};

    // pass resources

    Sampler *sampler;

    std::unique_ptr<DeferredData> deferred{};
    std::unique_ptr<AOData> ssao{};
    std::unique_ptr<SceneData> scene{};

    u32 current_ao_method = AO_METHOD::SSAO;
};
