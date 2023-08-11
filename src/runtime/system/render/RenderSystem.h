#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/memory/Memory.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/resource/resource_manager/ResourceManager.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/scene/camera/Camera.h>
#include <runtime/function/scene/scene_manager/SceneManager.h>

namespace Horizon {

class RenderSystem {
  public:
    RenderSystem(u32 width, u32 height, Window *window, RenderBackend backend, bool offscreen = false) noexcept;

    ~RenderSystem() noexcept;

    RenderSystem(const RenderSystem &rhs) noexcept = delete;

    RenderSystem &operator=(const RenderSystem &rhs) noexcept = delete;

    RenderSystem(RenderSystem &&rhs) noexcept = delete;

    RenderSystem &operator=(RenderSystem &&rhs) noexcept = delete;

    SceneManager *GetSceneManager() noexcept { return m_scene_manager.get(); };

  public:
    Backend::RHI *GetRhi() noexcept { return m_rhi.get(); }

  private:
    void InitializeRenderAPI(RenderBackend backend);

  private:
    Window *m_window{};
    std::unique_ptr<ResourceManager> m_resource_manager{};
    std::unique_ptr<SceneManager> m_scene_manager{};
    std::unique_ptr<Backend::RHI> m_rhi{};
};
} // namespace Horizon