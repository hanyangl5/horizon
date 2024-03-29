#include "Window.h"

#include <runtime/core/log/Log.h>

namespace Horizon {

Window::Window(const char *_name, u32 _width, u32 _height) noexcept : width(_width), height(_height) {
    if (glfwInit() != GLFW_TRUE) {
        glfwTerminate();
        LOG_ERROR("failed to init glfw");
    };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(static_cast<int>(_width), static_cast<int>(_height), _name, nullptr, nullptr);

    if (m_window == nullptr) {
        glfwTerminate();
        LOG_ERROR("failed to init window");
    }

    glfwSwapInterval(vsync_enabled ? 1 : 0);
    LOG_INFO("vsync stat: {}", vsync_enabled);
}
Window::~Window() noexcept {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

u32 Window::getWidth() const noexcept { return width; }

u32 Window::getHeight() const noexcept { return height; }

GLFWwindow *Window::getWindow() const noexcept { return m_window; }

int Window::ShouldClose() const noexcept { return glfwWindowShouldClose(m_window); }

void Window::close() noexcept { glfwSetWindowShouldClose(m_window, 1); }

} // namespace Horizon