/*****************************************************************/ /**
                                                                     * \file   Window.cpp
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#include "glfwwindow.h"

#include <core/log.h>

namespace Horizon
{
double g_glfw_lastFrameTime = 0.0;
unsigned int g_glfw_frameCount = 0;

Window::Window(const char *_name, u32 _width, u32 _height) noexcept : m_width(_width), m_height(_height)
{
#ifdef __ANDROID__
    // On Android, native window is set externally via SetNativeWindow()
    m_native_window = nullptr;
#else
    if (glfwInit() != GLFW_TRUE)
    {
        glfwTerminate();
        LOG_ERROR("failed to init glfw");
    };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(_width, _height, _name, nullptr, nullptr);

    if (!m_window)
    {
        glfwTerminate();
        LOG_ERROR("failed to init window");
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(0);
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
    m_width = static_cast<u32>(framebuffer_width > 0 ? framebuffer_width : 0);
    m_height = static_cast<u32>(framebuffer_height > 0 ? framebuffer_height : 0);
    // LOG_DEBUG("vsync : {}", m_vsync_enabled); // TODO(hylu): vsync not working now
#endif
}

Window::~Window() noexcept
{
#ifdef __ANDROID__
    // On Android, native window is managed by the system
    m_native_window = nullptr;
#else
    glfwDestroyWindow(m_window);
    glfwTerminate();
#endif
}

u32 Window::GetWidth() const noexcept
{
    return m_width;
}

u32 Window::GetHeight() const noexcept
{
    return m_height;
}

#ifdef __ANDROID__
ANativeWindow *Window::GetNativeWindow() const noexcept
{
    return m_native_window;
}

void Window::SetNativeWindow(ANativeWindow *window) noexcept
{
    m_native_window = window;
    if (window)
    {
        // Update dimensions from native window
        m_width = ANativeWindow_getWidth(window);
        m_height = ANativeWindow_getHeight(window);
    }
}
#else
GLFWwindow *Window::GetWindow() const noexcept
{
    return m_window;
}
#endif

void Window::UpdateWindowTitle(const char *title)
{
#ifndef __ANDROID__
    glfwSetWindowTitle(m_window, title);
#endif
}

void Window::ProcessEvents()
{
    // Process window events (mouse, keyboard, etc.)
#ifndef __ANDROID__
    glfwPollEvents();
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
    m_width = static_cast<u32>(framebuffer_width > 0 ? framebuffer_width : 0);
    m_height = static_cast<u32>(framebuffer_height > 0 ? framebuffer_height : 0);
#endif
}

int Window::ShouldClose() const noexcept
{
#ifdef __ANDROID__
    // On Android, check if native window is still valid
    return (m_native_window == nullptr);
#else
    return glfwWindowShouldClose(m_window);
#endif
}

void Window::Close() noexcept
{
#ifdef __ANDROID__
    m_native_window = nullptr;
#else
    glfwSetWindowShouldClose(m_window, true);
#endif
}

} // namespace Horizon
