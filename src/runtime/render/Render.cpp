#include "render.h"

#include <runtime/core/math/math.h>
#include <runtime/core/memory/memory.h>
#include <runtime/rhi/rhi.h>
#include <runtime/rhi/resourcebarrier.h>
#include <runtime/rhi/vulkan/rhivulkan.h>

namespace Horizon {

class Window;

Renderer::Renderer(const Config &config) noexcept {

    bool bOffScreen =
        config.app_type == ApplicationType::OFFSCREEN_GRAPHICS || config.app_type == ApplicationType::GENERAL_COMPUTE;
    if (!bOffScreen && config.window == nullptr) {
        LOG_ERROR("invalid window {}", (void *)config.window);
        return;
    }
    m_window = config.window;

    switch (config.render_backend) {
    case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
        mRhi = std::make_unique<Backend::RHIVulkan>(bOffScreen);
        break;
    default:
        return;
    }
    mRhi->InitializeRenderer();

    mRhi->SetWindow(m_window);

    m_resource_manager = std::make_unique<ResourceManager>(mRhi.get());

    m_scene_manager = std::make_unique<SceneManager>(m_resource_manager.get());
}

Renderer::~Renderer() noexcept {
    m_scene_manager = nullptr;
    m_resource_manager = nullptr;
    mRhi = nullptr;
}

} // namespace Horizon