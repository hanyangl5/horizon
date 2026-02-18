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

static bool g_app_initialized = false;
static std::thread *g_render_thread = nullptr;
static std::mutex g_app_mutex;

static void nativeOnCreate(JNIEnv *env, jobject thiz, jobject activity, jobject asset_manager)
{
    LOG_INFO("JNI: nativeOnCreate called");
    InitializeAndroidApp(env, activity, asset_manager);
}

static void nativeOnDestroy(JNIEnv *env, jobject thiz)
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

static void nativeOnPause(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnPause called");
    OnAppPause();
}

static void nativeOnResume(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnResume called");
    OnAppResume();
}

static void nativeOnSurfaceCreated(JNIEnv *env, jobject thiz, jobject surface)
{
    LOG_INFO("JNI: nativeOnSurfaceCreated called");
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window)
    {
        OnNativeWindowCreated(window);

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

static void nativeOnSurfaceDestroyed(JNIEnv *env, jobject thiz)
{
    LOG_INFO("JNI: nativeOnSurfaceDestroyed called");
    OnNativeWindowDestroyed();
}

static void nativeOnSurfaceChanged(JNIEnv *env, jobject thiz, jint width, jint height)
{
    LOG_INFO("JNI: nativeOnSurfaceChanged called: {}x{}", width, height);
    OnNativeWindowResized(static_cast<u32>(width), static_cast<u32>(height));
}

} // namespace Horizon

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        return JNI_ERR;
    }

    jclass cls = env->FindClass("com/horizon/engine/HorizonActivity");
    if (!cls)
    {
        return JNI_ERR;
    }

    // clang-format off
    const JNINativeMethod methods[] = {
        {"nativeOnCreate",           "(Landroid/app/Activity;Landroid/content/res/AssetManager;)V", reinterpret_cast<void *>(Horizon::nativeOnCreate)},
        {"nativeOnDestroy",          "()V",                                                        reinterpret_cast<void *>(Horizon::nativeOnDestroy)},
        {"nativeOnPause",            "()V",                                                        reinterpret_cast<void *>(Horizon::nativeOnPause)},
        {"nativeOnResume",           "()V",                                                        reinterpret_cast<void *>(Horizon::nativeOnResume)},
        {"nativeOnSurfaceCreated",   "(Landroid/view/Surface;)V",                                  reinterpret_cast<void *>(Horizon::nativeOnSurfaceCreated)},
        {"nativeOnSurfaceDestroyed", "()V",                                                        reinterpret_cast<void *>(Horizon::nativeOnSurfaceDestroyed)},
        {"nativeOnSurfaceChanged",   "(II)V",                                                      reinterpret_cast<void *>(Horizon::nativeOnSurfaceChanged)},
    };
    // clang-format on

    if (env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0])) < 0)
    {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

#endif // __ANDROID__
