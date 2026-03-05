#ifdef __ANDROID__

#include "android_native.h"
#include <android/native_window_jni.h>
#include <core/log.h>
#include <cstdlib>
#include <jni.h>
#include <mutex>
#include <string>
#include <thread>

namespace Horizon
{

extern "C" void HorizonRegisterAppEntryPoint() __attribute__((weak));

static bool g_app_initialized = false;
static std::thread *g_render_thread = nullptr;
static std::mutex g_app_mutex;

static std::string GetExternalFilesDirPath(JNIEnv *env, jobject activity)
{
    if (!env || !activity)
    {
        return {};
    }

    jclass activity_cls = env->GetObjectClass(activity);
    if (!activity_cls)
    {
        return {};
    }

    jmethodID get_external_files_dir =
        env->GetMethodID(activity_cls, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    if (!get_external_files_dir)
    {
        env->DeleteLocalRef(activity_cls);
        return {};
    }

    jobject file_obj = env->CallObjectMethod(activity, get_external_files_dir, nullptr);
    env->DeleteLocalRef(activity_cls);
    if (!file_obj)
    {
        return {};
    }

    jclass file_cls = env->FindClass("java/io/File");
    if (!file_cls)
    {
        env->DeleteLocalRef(file_obj);
        return {};
    }

    jmethodID get_absolute_path = env->GetMethodID(file_cls, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path)
    {
        env->DeleteLocalRef(file_cls);
        env->DeleteLocalRef(file_obj);
        return {};
    }

    auto path_jstr = static_cast<jstring>(env->CallObjectMethod(file_obj, get_absolute_path));
    std::string result;
    if (path_jstr)
    {
        const char *path_chars = env->GetStringUTFChars(path_jstr, nullptr);
        if (path_chars)
        {
            result = path_chars;
            env->ReleaseStringUTFChars(path_jstr, path_chars);
        }
        env->DeleteLocalRef(path_jstr);
    }

    env->DeleteLocalRef(file_cls);
    env->DeleteLocalRef(file_obj);
    return result;
}

static void nativeOnCreate(JNIEnv *env, jobject thiz, jobject activity, jobject asset_manager)
{
    LOG_INFO("JNI: nativeOnCreate called");
    InitializeAndroidApp(env, activity, asset_manager);
    const std::string external_files_dir = GetExternalFilesDirPath(env, activity);
    SetAndroidExternalFilesDir(external_files_dir.c_str());

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

    if (Horizon::HorizonRegisterAppEntryPoint)
    {
        Horizon::HorizonRegisterAppEntryPoint();
    }

    return JNI_VERSION_1_6;
}

#endif // __ANDROID__
