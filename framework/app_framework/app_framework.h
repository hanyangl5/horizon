/*****************************************************************/ /**
                                                                     * \file   app_framework.h
                                                                     * \brief  Application framework base class
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/
#pragma once

#include <core/glfwwindow.h>
#include <core/memory.h>
#include <rhi/enums.h>
#include <scene/scene_renderer/config.h>
#include <scene/scene_renderer/renderer.h>

#ifdef __ANDROID__
#include <chrono>
#include <core/android/android_native.h>
#include <thread>
#endif

namespace Horizon
{

class AppFramework
{
  public:
    AppFramework(const char *app_name, u32 default_width = 800, u32 default_height = 600);
    virtual ~AppFramework();

#ifdef __ANDROID__
    // Register this app instance as the entry point for Android
    void RegisterForAndroid();
#endif

    // Run the application
    void Run();

    // Parse CLI args like: -vk / -dx12 (or -dx)
    void ConfigureFromCommandLine(int argc, char **argv);

  protected:
    // Override these methods in derived classes
    virtual void Initialize() = 0;
    virtual void RenderLoop() = 0;
    virtual void Cleanup() = 0;
    // Called when framebuffer size changes. Default behavior recreates app resources.
    virtual void OnResize(u32 new_width, u32 new_height);

    // Accessors for derived classes
    Window *GetWindow() noexcept
    {
        return m_window.get();
    }
    Renderer *GetRenderer() noexcept
    {
        return m_renderer.get();
    }
    Backend::RHI *GetRhi() noexcept
    {
        return m_renderer ? m_renderer->GetRhi() : nullptr;
    }
    u32 GetWidth() const noexcept
    {
        return m_width;
    }
    u32 GetHeight() const noexcept
    {
        return m_height;
    }

    // Configuration
    void SetRenderBackend(RenderBackend backend)
    {
        m_render_backend = backend;
    }
    void SetApplicationType(ApplicationType type)
    {
        m_app_type = type;
    }

  private:
    void InitializeWindow();
    void InitializeRenderer();
    void ProcessEvents();
    bool ShouldContinue();

    const char *m_app_name;
    u32 m_width;
    u32 m_height;
    RenderBackend m_render_backend;
    ApplicationType m_app_type;

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    bool m_initialized{false};
};

} // namespace Horizon

#define DEFINE_HORIZON_APP(AppName) DEFINE_HORIZON_APP_WITH_CLASS(AppName, AppName##App)

#ifdef __ANDROID__
#define DEFINE_HORIZON_APP_WITH_CLASS(AppName, AppClass)                                                               \
    namespace                                                                                                          \
    {                                                                                                                  \
    static AppClass g_##AppName##_android_app_instance;                                                                \
    }                                                                                                                  \
    extern "C" void HorizonRegisterAppEntryPoint()                                                                     \
    {                                                                                                                  \
        Horizon::RegisterAppEntryPoint([]() { g_##AppName##_android_app_instance.Run(); });                            \
    }                                                                                                                  \
    void Run##AppName##App()                                                                                           \
    {                                                                                                                  \
        g_##AppName##_android_app_instance.Run();                                                                      \
    }
#else
#define DEFINE_HORIZON_APP_WITH_CLASS(AppName, AppClass)                                                               \
    int main(int argc, char **argv)                                                                                    \
    {                                                                                                                  \
        AppClass app;                                                                                                  \
        app.ConfigureFromCommandLine(argc, argv);                                                                      \
        app.Run();                                                                                                     \
        return 0;                                                                                                      \
    }
#endif
