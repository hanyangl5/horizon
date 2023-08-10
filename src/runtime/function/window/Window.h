#pragma once

#include <cstdint>
#include <memory>

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <runtime/core/math/Math.h>

namespace Horizon {

class Window {
  public:
    Window(const char *_name, u32 _width, u32 _height) noexcept;
    ~Window() noexcept;

    Window(const Window &) = default;
    Window(Window &&) = delete;
    Window &operator=(const Window &) = default;
    Window &operator=(Window &&) = delete;

    u32 getWidth() const noexcept;
    u32 getHeight() const noexcept;
    GLFWwindow *getWindow() const noexcept;
    int ShouldClose() const noexcept;
    void close() noexcept;

  private:
    GLFWwindow *m_window;
    u32 width, height;
    bool vsync_enabled = false;
};
} // namespace Horizon