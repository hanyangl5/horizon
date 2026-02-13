#ifdef __ANDROID__

#include "android_native.h"
#include <android/native_window_jni.h>
#include <core/log.h>
#include <cstdlib>
#include <jni.h>
#include <mutex>
#include <thread>

namespace Horizon
{

// JNI function declarations
extern "C"
{
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnCreate(JNIEnv *env, jobject thiz,
                                                                                      jobject activity,
                                                                                      jobject asset_manager);
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnDestroy(JNIEnv *env, jobject thiz);
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnPause(JNIEnv *env, jobject thiz);
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnResume(JNIEnv *env, jobject thiz);
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnSurfaceCreated(JNIEnv *env, jobject thiz,
                                                                                              jobject surface);
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnSurfaceDestroyed(JNIEnv *env,
                                                                                                jobject thiz);
    JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnSurfaceChanged(JNIEnv *env, jobject thiz,
                                                                                              jint width, jint height);
}

static bool g_app_initialized = false;
static std::thread *g_render_thread = nullptr;
static std::mutex g_app_mutex;

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnCreate(JNIEnv *env, jobject thiz,
                                                                                  jobject activity,
                                                                                  jobject asset_manager)
{
    LOG_INFO("JNI: nativeOnCreate called");
    InitializeAndroidApp(env, activity, asset_manager);
}

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnDestroy(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnDestroy called");
    OnAppDestroy();

    {
        std::lock_guard<std::mutex> lock(g_app_mutex);
        if (g_render_thread && g_render_thread->joinable())
        {
            g_render_thread->join();
            delete g_render_thread;
            g_render_thread = nullptr;
        }
        g_app_initialized = false;
    }

    CleanupAndroidApp();
}

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnPause(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnPause called");
    OnAppPause();
}

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnResume(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnResume called");
    OnAppResume();
}

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnSurfaceCreated(JNIEnv *env, jobject thiz,
                                                                                          jobject surface)
{
    LOG_INFO("JNI: nativeOnSurfaceCreated called");
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window)
    {
        OnNativeWindowCreated(window);

        // Start render thread if not already started
        {
            std::lock_guard<std::mutex> lock(g_app_mutex);
            if (!g_app_initialized)
            {
                g_app_initialized = true;
                g_render_thread = new std::thread([]() {
                    LOG_INFO("Starting render thread");
                    RunAppEntryPoint();
                });
            }
        }
    }
    else
    {
        LOG_ERROR("Failed to get native window from surface");
    }
}

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnSurfaceDestroyed(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnSurfaceDestroyed called");
    OnNativeWindowDestroyed();
}

JNIEXPORT void JNICALL Java_com_horizon_hellotriangle_MainActivity_nativeOnSurfaceChanged(JNIEnv *env, jobject thiz,
                                                                                          jint width, jint height)
{
    LOG_INFO("JNI: nativeOnSurfaceChanged called: {}x{}", width, height);
    OnNativeWindowResized(static_cast<u32>(width), static_cast<u32>(height));
}

} // namespace Horizon

#endif // __ANDROID__
