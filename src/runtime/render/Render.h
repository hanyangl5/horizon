#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/memory/Memory.h>
#include <runtime/core/window/Window.h>

#include <runtime/render/Config.h>
#include <runtime/resource/resource_manager/ResourceManager.h>
#include <runtime/rhi/RHI.h>
#include <runtime/scene/camera/Camera.h>
#include <runtime/scene/scene_manager/SceneManager.h>
namespace Horizon {

class Renderer {
  public:
    Renderer(const Config &config) noexcept;

    ~Renderer() noexcept;

    Renderer(const Renderer &rhs) noexcept = delete;

    Renderer &operator=(const Renderer &rhs) noexcept = delete;

    Renderer(Renderer &&rhs) noexcept = delete;

    Renderer &operator=(Renderer &&rhs) noexcept = delete;

    SceneManager *GetSceneManager() noexcept { return m_scene_manager.get(); };

    void BeginNewFrame() const {
        m_rhi->ResetRHIResources();
        m_rhi->ResetFence(CommandQueueType::GRAPHICS);
        m_rhi->ResetFence(CommandQueueType::COMPUTE);
        m_rhi->ResetFence(CommandQueueType::TRANSFER);
        LOG_DEBUG("begin frame");
    };
    void EndFrame() const {
        m_rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
        m_rhi->WaitGpuExecution(CommandQueueType::COMPUTE);
        m_rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
        LOG_DEBUG("end frame");
    };

  public:
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