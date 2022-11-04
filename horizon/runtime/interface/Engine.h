#pragma once

#include <runtime/core/memory/Alloc.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/scene/scene_manager/SceneManager.h>

#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>

#include <runtime/interface/EngineConfig.h>
namespace Horizon {
class Engine final {
  public:
    Engine(const EngineConfig &config) noexcept;
    ~Engine() noexcept;

    Engine(const Engine &rhs) noexcept = delete;
    Engine &operator=(const Engine &rhs) noexcept = delete;
    Engine(Engine &&rhs) noexcept = delete;
    Engine &operator=(Engine &&rhs) noexcept = delete;

    void BeginNewFrame() const;
    void EndFrame() const;

  public:
    Memory::UniquePtr<Window> m_window{};
    Memory::UniquePtr<RenderSystem> m_render_system{};
    Memory::UniquePtr<InputSystem> m_input_system{};
};

} // namespace Horizon
