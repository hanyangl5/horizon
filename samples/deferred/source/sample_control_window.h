#pragma once

#include <functional>

#include <core/definations.h>
#include <core/glfwwindow.h>

class SampleControlWindow
{
  public:
    using VSyncCallback = std::function<void(bool)>;

    SampleControlWindow() = default;
    ~SampleControlWindow();

    void Initialize(Horizon::Window *target_window, const VSyncCallback &vsync_callback);
    void Shutdown();
    void RenderFrame(bool vsync_enabled);

  private:
    Horizon::Window *m_target_window{};
    VSyncCallback m_vsync_callback{};

#ifndef __ANDROID__
    struct GLFWwindow *m_window{};
#endif
    bool m_initialized{false};
};
