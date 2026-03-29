/*****************************************************************/ /**
                                                                     * \file   Window.h
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#pragma once

#include <cstdint>

#ifdef __ANDROID__
#include <android/native_window.h>
#else
#include <SDL3/SDL.h>
#endif

#include <core/definations.h>

namespace Horizon
{

class Window
{
  public:
    Window(const char *_name, u32 _width, u32 _height, bool enable_vulkan_surface = false) noexcept;
    ~Window() noexcept;

    u32 GetWidth() const noexcept;
    u32 GetHeight() const noexcept;

#ifdef __ANDROID__
    ANativeWindow *GetNativeWindow() const noexcept;
    void SetNativeWindow(ANativeWindow *window) noexcept;
#else
    SDL_Window *GetSDLWindow() const noexcept;
    void *GetNativeWindow() const noexcept;
#if defined(__APPLE__) && defined(USE_METAL)
    void *GetNativeView() const noexcept;
#endif
    void HandleSDLEvent(const SDL_Event &event) noexcept;
#endif

    int ShouldClose() const noexcept;
    void Close() noexcept;
    void UpdateWindowTitle(const char *title);
    void SetWindowSize(u32 width, u32 height);
    void ProcessEvents();

  private:
    void UpdatePixelSize() noexcept;

#ifdef __ANDROID__
    ANativeWindow *m_native_window{};
#else
    SDL_Window *m_window{};
#if defined(__APPLE__) && defined(USE_METAL)
    void *m_view{};
#endif
    bool m_should_close{false};
    bool m_enable_vulkan_surface{false};
    SDL_WindowID m_window_id{};
#endif

    u32 m_width{};
    u32 m_height{};
};

} // namespace Horizon
