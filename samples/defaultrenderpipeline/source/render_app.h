
#pragma once

#include "ambient_occlusion.h"
#include "antialiasing.h"
#include "deferredshading.h"
#include "post_process.h"
#include "scene.h"
#include <render_graph/frame_graph.h>
#include <scene/scene_renderer/renderer.h>
// Render

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

        deferred = nullptr;
        ssao = nullptr;
        post_process = nullptr;
        antialiasing = nullptr;
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
    // add shadow map pass
    // mesh shader cull
    std::unique_ptr<DeferredShadingPass> deferred{};
    // hzb
    std::unique_ptr<AmbientOcclusionPass> ssao{};
    // std::unique_ptr<ReflectionPass> reflection{};
    // std::unique_ptr<Atmosphere> reflection{};
    // std::unique_ptr<VolumetricFog> reflection{};
    // std::unique_ptr<VolumetricCloud> reflection{}; // screenspace/rtx
    // combination
    std::unique_ptr<AntialiasingPass> antialiasing{};
    std::unique_ptr<PostProcessingPass> post_process{};

    std::unique_ptr<SceneData> scene{};
};
