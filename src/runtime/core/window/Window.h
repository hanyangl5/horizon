/*****************************************************************/ /**
 * \file   Window.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <cstdint>

// include windows.h before glfw3 to prevent compile warning
// https://social.msdn.microsoft.com/Forums/en-US/7d5d7d91-5ec3-42fb-a2fd-c52b5e01349b/c4005-apientry-makro-neudefinition-in-minwindefh-and-thus-2-unknown-identifier?forum=vcgeneral

#include <GLFW/glfw3.h>

#include <runtime/core/utils/definations.h>

namespace Horizon {

class Window {
  public:
    Window(const char *_name, u32 _width, u32 _height) noexcept;
    ~Window() noexcept;

    u32 GetWidth() const noexcept;
    u32 GetHeight() const noexcept;
    GLFWwindow *GetWindow() const noexcept;

    int ShouldClose() const noexcept;
    void Close() noexcept;
    void UpdateWindowTitle(const char *title);

  private:
    GLFWwindow *m_window{};
    u32 m_width{};
    u32 m_height{};
    bool m_vsync_enabled = true;
};

} // namespace Horizon