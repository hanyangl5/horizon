/*****************************************************************/ /**
                                                                     * \file   Window.cpp
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#include "window.h"

#include <core/log.h>

#include <mutex>

#if defined(__APPLE__) && defined(USE_METAL)
#include <SDL3/SDL_metal.h>
#endif

namespace Horizon
{

#ifndef __ANDROID__
namespace
{
std::mutex g_sdl_init_mutex;
u32 g_sdl_window_ref_count = 0;

bool AcquireSDLVideo() noexcept
{
    std::lock_guard<std::mutex> lock(g_sdl_init_mutex);
    if (g_sdl_window_ref_count == 0)
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            LOG_ERROR("failed to init sdl3: {}", SDL_GetError());
            return false;
        }
    }

    ++g_sdl_window_ref_count;
    return true;
}

void ReleaseSDLVideo() noexcept
{
    std::lock_guard<std::mutex> lock(g_sdl_init_mutex);
    if (g_sdl_window_ref_count == 0)
    {
        return;
    }

    --g_sdl_window_ref_count;
    if (g_sdl_window_ref_count == 0)
    {
        SDL_Quit();
    }
}
} // namespace
#endif

Window::Window(const char *_name, u32 _width, u32 _height, bool enable_vulkan_surface) noexcept
    : m_width(_width), m_height(_height)
{
#ifdef __ANDROID__
    // On Android, native window is set externally via SetNativeWindow()
    m_native_window = nullptr;
#else
    m_enable_vulkan_surface = enable_vulkan_surface;

    if (!AcquireSDLVideo())
    {
        return;
    }

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (m_enable_vulkan_surface)
    {
        window_flags |= SDL_WINDOW_VULKAN;
    }
#if defined(__APPLE__) && defined(USE_METAL)
    window_flags |= SDL_WINDOW_METAL;
#endif

    m_window = SDL_CreateWindow(_name != nullptr ? _name : "Horizon", static_cast<int>(_width),
                                static_cast<int>(_height), window_flags);

    if (!m_window)
    {
        LOG_ERROR("failed to init window: {}", SDL_GetError());
        ReleaseSDLVideo();
        return;
    }

    m_window_id = SDL_GetWindowID(m_window);

#if defined(__APPLE__) && defined(USE_METAL)
    if (m_window != nullptr)
    {
        m_view = SDL_Metal_CreateView(m_window);
        if (m_view == nullptr)
        {
            LOG_ERROR("failed to create metal view: {}", SDL_GetError());
        }
    }
#endif

    UpdatePixelSize();
#endif
}

Window::~Window() noexcept
{
#ifdef __ANDROID__
    // On Android, native window is managed by the system
    m_native_window = nullptr;
#else
#if defined(__APPLE__) && defined(USE_METAL)
    if (m_view != nullptr)
    {
        SDL_Metal_DestroyView(m_view);
        m_view = nullptr;
    }
#endif
    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    ReleaseSDLVideo();
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
SDL_Window *Window::GetSDLWindow() const noexcept
{
    return m_window;
}

void *Window::GetNativeWindow() const noexcept
{
    if (m_window == nullptr)
    {
        return nullptr;
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
#if defined(_WIN32)
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#else
    (void)props;
    return nullptr;
#endif
}

#if defined(__APPLE__) && defined(USE_METAL)
void *Window::GetNativeView() const noexcept
{
    return m_view;
}
#endif

void Window::HandleSDLEvent(const SDL_Event &event) noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    if (event.type == SDL_EVENT_QUIT)
    {
        m_should_close = true;
        return;
    }

    if (event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
        event.type != SDL_EVENT_WINDOW_RESIZED)
    {
        return;
    }

    if (event.window.windowID != m_window_id)
    {
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        m_should_close = true;
        return;
    }

    UpdatePixelSize();
}
#endif

void Window::UpdateWindowTitle(const char *title)
{
#ifndef __ANDROID__
    if (m_window == nullptr || title == nullptr)
    {
        return;
    }
    SDL_SetWindowTitle(m_window, title);
#endif
}

void Window::SetWindowSize(u32 width, u32 height)
{
#ifndef __ANDROID__
    if (m_window == nullptr)
    {
        return;
    }
    SDL_SetWindowSize(m_window, static_cast<int>(width), static_cast<int>(height));
    UpdatePixelSize();
#else
    (void)width;
    (void)height;
#endif
}

void Window::ProcessEvents()
{
    // Process window events (mouse, keyboard, etc.)
#ifndef __ANDROID__
    UpdatePixelSize();
#endif
}

int Window::ShouldClose() const noexcept
{
#ifdef __ANDROID__
    // On Android, check if native window is still valid
    return (m_native_window == nullptr);
#else
    return m_should_close ? 1 : 0;
#endif
}

void Window::Close() noexcept
{
#ifdef __ANDROID__
    m_native_window = nullptr;
#else
    m_should_close = true;
#endif
}

void Window::UpdatePixelSize() noexcept
{
#ifndef __ANDROID__
    if (m_window == nullptr)
    {
        m_width = 0;
        m_height = 0;
        return;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    if (!SDL_GetWindowSizeInPixels(m_window, &framebuffer_width, &framebuffer_height))
    {
        LOG_WARN("failed to query window pixel size: {}", SDL_GetError());
        m_width = 0;
        m_height = 0;
        return;
    }

    m_width = static_cast<u32>(framebuffer_width > 0 ? framebuffer_width : 0);
    m_height = static_cast<u32>(framebuffer_height > 0 ? framebuffer_height : 0);
#endif
}

} // namespace Horizon
