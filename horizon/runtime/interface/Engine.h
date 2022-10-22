#pragma once

#include <memory>

#include <BS_thread_pool.hpp>

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/interface/EngineConfig.h>

#include <runtime/function/scene/scene_manager/SceneManager.h>
#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>

namespace Horizon {
class Engine final {
  public:
    Engine(const EngineConfig &config) noexcept;
    ~Engine() noexcept = default;

    Engine(const Engine &rhs) noexcept = delete;
    Engine &operator=(const Engine &rhs) noexcept = delete;
    Engine(Engine &&rhs) noexcept = delete;
    Engine &operator=(Engine &&rhs) noexcept = delete;

    void BeginNewFrame() const;
    void EndFrame() const;

  public:
    std::unique_ptr<Window> m_window{};
    std::unique_ptr<RenderSystem> m_render_system{};
    std::unique_ptr<InputSystem> m_input_system{};

};

} // namespace Horizon
