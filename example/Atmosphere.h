#pragma once

#include <memory>

#include <runtime/function/input/InputManager.h>
#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/window/Window.h>
#include <runtime/scene/render/Renderer.h>


class App {
  public:
  
    App(Horizon::u32 _width, Horizon::u32 _height) noexcept;
    ~App() noexcept = default;
    App(const App &) = delete;
    App(App &&) = delete;
    App &operator=(const App &) = delete;
    App &operator=(App &&) = delete;

    void Run() noexcept;

  private:
    Horizon::u32 m_width;
    Horizon::u32 mHeight;
    std::shared_ptr<Horizon::Window> m_window = nullptr;
    std::unique_ptr<Horizon::Renderer> m_renderer = nullptr;
    std::unique_ptr<Horizon::InputManager> m_input_manager;
};
