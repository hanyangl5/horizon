#include "renderer.h"

#include <core/math.h>
#include <core/memory.h>
#include <rhi/resource_barrier.h>
#include <rhi/rhi.h>

namespace Horizon
{

class Window;

Renderer::Renderer(const Config &config) noexcept
{

    bool bOffScreen =
        config.app_type == ApplicationType::OFFSCREEN_GRAPHICS || config.app_type == ApplicationType::GENERAL_COMPUTE;
    if (!bOffScreen && config.window == nullptr)
    {
        LOG_ERROR("invalid window {}", (void *)config.window);
        return;
    }
    m_window = config.window;

    mRhi = Horizon::Backend::CreateRenderBackend(config.render_backend, bOffScreen);
    mRhi->InitializeRenderer();

    mRhi->SetWindow(m_window);

    m_resource_manager = std::make_unique<ResourceManager>(mRhi.get());

    m_scene_manager = std::make_unique<SceneManager>(m_resource_manager.get());
}

Renderer::~Renderer() noexcept
{
    m_scene_manager = nullptr;
    m_resource_manager = nullptr;
    mRhi = nullptr;
}

} // namespace Horizon