#pragma once

#include <cstdint>
#include <memory>

// include windows.h before glfw3 to prevent compile warning
// https://social.msdn.microsoft.com/Forums/en-US/7d5d7d91-5ec3-42fb-a2fd-c52b5e01349b/c4005-apientry-makro-neudefinition-in-minwindefh-and-thus-2-unknown-identifier?forum=vcgeneral

#ifdef _WIN32
#include <Windows.h>
#endif

#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <runtime/core/utils/Definations.h>

namespace Horizon {

class Window {
  public:
    Window(const char *_name, u32 _width, u32 _height) noexcept;

    ~Window() noexcept;

    Window(const Window &rhs) noexcept = delete;

    Window &operator=(const Window &rhs) noexcept = delete;

    Window(Window &&rhs) noexcept = delete;

    Window &operator=(Window &&rhs) noexcept = delete;

  public:
    u32 GetWidth() const noexcept;
    u32 GetHeight() const noexcept;
    GLFWwindow *GetWindow() const noexcept;
#ifdef _WIN32
    HWND GetWin32Window() const noexcept {
        return glfwGetWin32Window(m_window);
    }
#endif
    int ShouldClose() const noexcept;
    void close() noexcept;

  private:
    GLFWwindow *m_window;
    u32 width, height;
    bool vsync_enabled = true;
};
} // namespace Horizon