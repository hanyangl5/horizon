#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/scene/camera/Camera.h>

namespace Horizon {

using Buffer = RHI::Buffer;
using Texture = RHI::Texture;
using RenderTarget = RHI::RenderTarget;
using Shader = RHI::Shader;
using Pipeline = RHI::Pipeline;
using CommandList = RHI::CommandList;

class RenderSystem {
  public:
    RenderSystem(u32 width, u32 height, Window *window, RenderBackend backend) noexcept;

    ~RenderSystem() noexcept;

    RenderSystem(const RenderSystem &rhs) noexcept = delete;

    RenderSystem &operator=(const RenderSystem &rhs) noexcept = delete;

    RenderSystem(RenderSystem &&rhs) noexcept = delete;

    RenderSystem &operator=(RenderSystem &&rhs) noexcept = delete;

  public:
    Camera *GetMainCamera() const;

    RHI::RHI *GetRhi() noexcept { return m_rhi.get(); }

  private:
    void InitializeRenderAPI(RenderBackend backend);

  private:
    Window *m_window{};
    std::unique_ptr<Camera> m_debug_camera;
    std::unique_ptr<RHI::RHI> m_rhi{};
};
} // namespace Horizon