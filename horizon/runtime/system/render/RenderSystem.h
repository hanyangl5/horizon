#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/scene/camera/Camera.h>

namespace Horizon {

class RenderSystem {
  public:
    RenderSystem(u32 width, u32 height, Window *window, RenderBackend backend, bool offscreen = false) noexcept;

    ~RenderSystem() noexcept;

    RenderSystem(const RenderSystem &rhs) noexcept = delete;

    RenderSystem &operator=(const RenderSystem &rhs) noexcept = delete;

    RenderSystem(RenderSystem &&rhs) noexcept = delete;

    RenderSystem &operator=(RenderSystem &&rhs) noexcept = delete;

  public:
    void SetCamera(Camera *camera) noexcept { m_debug_camera = camera; }

    Camera *GetDebugCamera() const;

    Backend::RHI *GetRhi() noexcept { return m_rhi.get(); }

  private:
    void InitializeRenderAPI(RenderBackend backend);

  private:
    Window *m_window{};
    Camera *m_debug_camera{};
    std::unique_ptr<Backend::RHI> m_rhi{};
};
} // namespace Horizon