/*****************************************************************//**
 * \file   window.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "Window.h"

#include <runtime/core/log/Log.h>

namespace Horizon {

Window::Window(const char *_name, u32 _width, u32 _height) noexcept : m_width(_width), m_height(_height) {
    if (glfwInit() != GLFW_TRUE) {
        glfwTerminate();
        LOG_ERROR("failed to init glfw");
    };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(_width, _height, _name, nullptr, nullptr);

    if (!m_window) {
        glfwTerminate();
        LOG_ERROR("failed to init window");
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(m_vsync_enabled ? 1 : 0);
    LOG_DEBUG("vsync : {}", m_vsync_enabled); // TODO(hylu): vsync not working now
}

Window::~Window() noexcept {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

u32 Window::GetWidth() const noexcept {
    return m_width; 
}

u32 Window::GetHeight() const noexcept {
    return m_height; 
}

GLFWwindow *Window::GetWindow() const noexcept {
    return m_window; 
}

int Window::ShouldClose() const noexcept {
    return glfwWindowShouldClose(m_window); 
}

void Window::Close() noexcept {
    glfwSetWindowShouldClose(m_window, true); 
}

} // namespace Horizon