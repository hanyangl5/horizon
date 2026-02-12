#pragma once

#ifdef __ANDROID__

#include <jni.h>
#include <android/native_window.h>
#include <core/definations.h>

namespace Horizon
{

// Android application state
struct AndroidAppState
{
    ANativeWindow *native_window{nullptr};
    u32 width{0};
    u32 height{0};
    bool window_ready{false};
    bool paused{false};
    bool destroyed{false};
};

// Get global Android application state
AndroidAppState &GetAndroidAppState();

// Initialize Android application
void InitializeAndroidApp(JNIEnv *env, jobject activity, jobject asset_manager);

// Cleanup Android application
void CleanupAndroidApp();

// Handle window creation
void OnNativeWindowCreated(ANativeWindow *window);

// Handle window destruction
void OnNativeWindowDestroyed();

// Handle window resize
void OnNativeWindowResized(u32 width, u32 height);

// Handle app pause
void OnAppPause();

// Handle app resume
void OnAppResume();

// Handle app destroy
void OnAppDestroy();

// Application entry point callback type
// Using std::function to allow capturing lambdas
#include <functional>
using AppEntryPoint = std::function<void()>;

// Register application entry point (called from app layer)
void RegisterAppEntryPoint(AppEntryPoint entry_point);

// Run application entry point (called from framework when window is ready)
void RunAppEntryPoint();

} // namespace Horizon

#endif // __ANDROID__
