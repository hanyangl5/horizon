/*****************************************************************/ /**
                                                                     * \file   app_framework.cpp
                                                                     * \brief  Application framework base class
                                                                     *implementation
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#include "app_framework.h"
#include <chrono>
#include <core/log.h>
#include <core/path.h>
#include <cstring>
#include <thread>
#ifdef __ANDROID__
#include <core/android/android_native.h>
#endif

namespace Horizon
{

AppFramework::AppFramework(const char *app_name, u32 default_width, u32 default_height)
    : m_app_name(app_name), m_width(default_width), m_height(default_height),
      m_render_backend(RenderBackend::RENDER_BACKEND_VULKAN), m_app_type(ApplicationType::GRAPHICS)
{
#ifdef __ANDROID__
    // Automatically register this app instance for Android
    RegisterForAndroid();
#endif
}

AppFramework::~AppFramework()
{
    // Note: We don't call Cleanup() here because:
    // 1. Virtual function calls in destructors are unsafe (derived class may be partially destroyed)
    // 2. Cleanup() should be called explicitly before destruction, or the derived class
    //    should handle cleanup in its own destructor
    // If cleanup is needed, derived classes should call Cleanup() in their destructor
    // before the base class destructor runs
}

void AppFramework::ConfigureFromCommandLine(int argc, char **argv)
{
#ifdef __ANDROID__
    (void)argc;
    (void)argv;
    return;
#else
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (!arg)
        {
            continue;
        }

        if (strcmp(arg, "-vk") == 0 || strcmp(arg, "--vk") == 0 || strcmp(arg, "-vulkan") == 0 ||
            strcmp(arg, "--vulkan") == 0)
        {
            SetRenderBackend(RenderBackend::RENDER_BACKEND_VULKAN);
            continue;
        }

        if (strcmp(arg, "-dx") == 0 || strcmp(arg, "--dx") == 0 || strcmp(arg, "-dx12") == 0 ||
            strcmp(arg, "--dx12") == 0)
        {
            SetRenderBackend(RenderBackend::RENDER_BACKEND_DX12);
            continue;
        }
    }
#endif
}

void AppFramework::Run()
{

#ifdef __ANDROID__
    // Single place for setting log file sink (desktop and Android; on Android external_files_dir is set before Run()).
    Log::GetInstance().SetFileSink(Horizon::Path::log_file_path().string());
    // Wait for native window to be ready
    while (!GetAndroidAppState().window_ready || GetAndroidAppState().native_window == nullptr)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        if (GetAndroidAppState().destroyed)
        {
            LOG_INFO("App destroyed before window ready");
            return;
        }
    }

    // Update dimensions from Android state
    auto &android_state = GetAndroidAppState();
    m_width = android_state.width;
    m_height = android_state.height;
#endif

    InitializeWindow();
    InitializeRenderer();

    if (!m_renderer || !m_renderer->GetRhi())
    {
        LOG_ERROR("Failed to initialize renderer");
        return;
    }

    m_initialized = true;
    Initialize();
    u32 previous_width = m_width;
    u32 previous_height = m_height;

    LOG_INFO("{} initialized. Starting render loop...", m_app_name);

    // Main render loop
    while (ShouldContinue())
    {
        ProcessEvents();

        if (!ShouldContinue())
        {
            break;
        }

        if (m_width != previous_width || m_height != previous_height)
        {
            if (m_width > 0 && m_height > 0)
            {
                OnResize(m_width, m_height);
            }
            previous_width = m_width;
            previous_height = m_height;
        }

        // Skip rendering while minimized (e.g. framebuffer size is 0x0 on desktop).
        // This prevents invalid render area/viewport sizes from reaching backend command recording.
        if (m_width == 0 || m_height == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        // Render frame
        RenderLoop();
    }

    LOG_INFO("{} finished. clean resources...", m_app_name);
    Cleanup();
}

void AppFramework::OnResize(u32 new_width, u32 new_height)
{
    (void)new_width;
    (void)new_height;
}

void AppFramework::InitializeWindow()
{
    m_window = std::make_unique<Window>(m_app_name, m_width, m_height);

#ifdef __ANDROID__
    while (!GetAndroidAppState().destroyed && GetAndroidAppState().native_window == nullptr)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    auto &android_state = GetAndroidAppState();
    if (android_state.native_window)
    {
        m_window->SetNativeWindow(android_state.native_window);
    }
#endif
}

void AppFramework::InitializeRenderer()
{
    Config config{};
    config.width = m_width;
    config.height = m_height;
    config.render_backend = m_render_backend;
    config.app_type = m_app_type;
    config.window = m_window.get();

    m_renderer = std::make_unique<Renderer>(config);
}

void AppFramework::ProcessEvents()
{
#ifdef __ANDROID__
    // Check if app is paused or destroyed
    auto &android_state = GetAndroidAppState();
    if (android_state.paused || android_state.destroyed)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }

    // Update window size if changed
    if (android_state.window_ready && android_state.native_window)
    {
        u32 new_width = android_state.width;
        u32 new_height = android_state.height;
        if (new_width != m_width || new_height != m_height)
        {
            m_width = new_width;
            m_height = new_height;
            // TODO: Handle resize
        }
    }
#else
    m_window->ProcessEvents();
    m_width = m_window->GetWidth();
    m_height = m_window->GetHeight();
#endif
}

bool AppFramework::ShouldContinue()
{
    if (!m_window)
    {
        return false;
    }

#ifdef __ANDROID__
    auto &android_state = GetAndroidAppState();
    if (android_state.destroyed || !android_state.window_ready)
    {
        return false;
    }
#endif

    return !m_window->ShouldClose();
}

#ifdef __ANDROID__
void AppFramework::RegisterForAndroid()
{
    // Create a lambda that captures 'this' and calls Run()
    // This lambda will be stored in the framework and called from JNI
    RegisterAppEntryPoint([this]() { this->Run(); });
}
#endif

} // namespace Horizon
