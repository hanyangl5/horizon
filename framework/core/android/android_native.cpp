#ifdef __ANDROID__

#include "android_native.h"
#include <core/log.h>
#include <mutex>

namespace Horizon
{

static AndroidAppState g_android_state{};
static std::mutex g_android_mutex;
static AppEntryPoint g_app_entry_point;

AndroidAppState &GetAndroidAppState()
{
    return g_android_state;
}

void InitializeAndroidApp(JNIEnv *env, jobject activity, jobject asset_manager)
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.native_window = nullptr;
    g_android_state.width = 0;
    g_android_state.height = 0;
    g_android_state.window_ready = false;
    g_android_state.paused = false;
    g_android_state.destroyed = false;
    g_android_state.external_files_dir.clear();
    LOG_INFO("Android app initialized");
    // Store JNI references if needed
    (void)env;
    (void)activity;
    (void)asset_manager;
}

void CleanupAndroidApp()
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.native_window = nullptr;
    g_android_state.window_ready = false;
    LOG_INFO("Android app cleaned up");
}

void OnNativeWindowCreated(ANativeWindow *window)
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.native_window = window;
    if (window)
    {
        g_android_state.width = ANativeWindow_getWidth(window);
        g_android_state.height = ANativeWindow_getHeight(window);
        g_android_state.window_ready = true;
        LOG_INFO("Native window created: {}x{}", g_android_state.width, g_android_state.height);
    }
}

void OnNativeWindowDestroyed()
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.native_window = nullptr;
    g_android_state.window_ready = false;
    LOG_INFO("Native window destroyed");
}

void OnNativeWindowResized(u32 width, u32 height)
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.width = width;
    g_android_state.height = height;
    if (g_android_state.native_window)
    {
        g_android_state.width = ANativeWindow_getWidth(g_android_state.native_window);
        g_android_state.height = ANativeWindow_getHeight(g_android_state.native_window);
    }
    LOG_INFO("Native window resized: {}x{}", width, height);
}

void OnAppPause()
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.paused = true;
    LOG_INFO("App paused");
}

void OnAppResume()
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.paused = false;
    LOG_INFO("App resumed");
}

void OnAppDestroy()
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.destroyed = true;
    g_android_state.window_ready = false;
    g_android_state.native_window = nullptr;
    LOG_INFO("App destroyed");
}

void SetAndroidExternalFilesDir(const char *path)
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_android_state.external_files_dir = (path != nullptr) ? path : "";
    LOG_INFO("Android external files dir: {}", g_android_state.external_files_dir);
    // Log file sink is set once in AppFramework::Run() after external_files_dir is available.
}

const std::string &GetAndroidExternalFilesDir()
{
    return g_android_state.external_files_dir;
}

void RegisterAppEntryPoint(AppEntryPoint entry_point)
{
    std::lock_guard<std::mutex> lock(g_android_mutex);
    g_app_entry_point = entry_point;
    LOG_INFO("App entry point registered");
}

void RunAppEntryPoint()
{
    AppEntryPoint entry_point;
    {
        std::lock_guard<std::mutex> lock(g_android_mutex);
        entry_point = g_app_entry_point;
    }

    if (entry_point)
    {
        entry_point();
    }
    else
    {
        LOG_ERROR("App entry point not registered");
    }
}

} // namespace Horizon

#endif // __ANDROID__
