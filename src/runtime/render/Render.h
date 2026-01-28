#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/memory/memory.h>
#include <runtime/core/window/window.h>

#include <runtime/render/config.h>
#include <runtime/resource/resource_manager/resourcemanager.h>
#include <runtime/rhi/rhi.h>
#include <runtime/scene/camera/camera.h>
#include <runtime/scene/scene_manager/scenemanager.h>
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
        mRhi->ResetRHIResources();
        mRhi->ResetFence(CommandQueueType::GRAPHICS);
        mRhi->ResetFence(CommandQueueType::COMPUTE);
        mRhi->ResetFence(CommandQueueType::TRANSFER);
        LOG_DEBUG("begin frame");
    };
    void EndFrame() const {
        mRhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
        mRhi->WaitGpuExecution(CommandQueueType::COMPUTE);
        mRhi->WaitGpuExecution(CommandQueueType::TRANSFER);
        LOG_DEBUG("end frame");
    };

  public:
    Backend::RHI *GetRhi() noexcept { return mRhi.get(); }

  private:
    Window *m_window{};
    std::unique_ptr<ResourceManager> m_resource_manager{};
    std::unique_ptr<SceneManager> m_scene_manager{};
    std::unique_ptr<Backend::RHI> mRhi{};
};
} // namespace Horizon