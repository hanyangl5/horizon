/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include "Core/IConfig.h"

#include "../Private/CPUConfig.h"

#if defined(_WINDOWS) || defined(XBOX)
#include <stdlib.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#if defined(__APPLE__)
#if !defined(TARGET_IOS)
#import <Carbon/Carbon.h>
#else
#include <stdint.h>
typedef uint64_t uint64;
#endif

// Support fixed set of OS versions in order to minimize maintenance efforts
// Two runtimes. Using IOS in the name as iOS versions are easier to remember
// Try to match the macOS version with the relevant features in iOS version
#define IOS14_API     API_AVAILABLE(macos(11.0), ios(14.1))
#define IOS14_RUNTIME @available(macOS 11.0, iOS 14.1, *)

#define IOS17_API     API_AVAILABLE(macos(14.0), ios(17.0))
#define IOS17_RUNTIME @available(macOS 14.0, iOS 17.0, *)

#elif defined(__ANDROID__)
#include <android/log.h>
#include <android_native_app_glue.h>

#elif defined(__linux__) && !defined(VK_USE_PLATFORM_GGP)
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
// X11 defines primitive types which conflict with Forge libraries
#undef Bool
#endif

#elif defined(NX64)
#include "../../../Switch/Common_3/OS/NX/NXTypes.h"

#elif defined(ORBIS)
#define THREAD_STACK_SIZE_ORBIS (64u * Kilobyte)
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// For time related functions such as getting localtime
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#if defined(FORGE_DEBUG) && (defined(_WINDOWS) || (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__))
#define WINDOW_DETAILS 1
#include <ThirdParty/bstrlib/bstrlib.h>
#else
#define WINDOW_DETAILS 0
#endif

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

#if !defined(_WINDOWS) && !defined(XBOX)
#include <unistd.h>
#define stricmp(a, b) strcasecmp(a, b)
#if !defined(ORBIS) && !defined(PROSPERO)
#define vsprintf_s vsnprintf
#endif
#endif

#if defined(XBOX)
#define stricmp(a, b) _stricmp(a, b)
#endif

//------------------------------------------------------------------------
// WINDOW AND RESOLUTION
//------------------------------------------------------------------------

#define MAX_MONITOR_COUNT 32

typedef enum WindowMode
{
    WindowModeWindowed,
    WindowModeFullscreen,
    WindowModeBorderless
} WindowMode;

typedef struct RectDesc
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} RectDesc;

typedef enum WindowHandleType
{
    WindowHandleTypeUnknown,
    WindowHandleTypeWin32,
    WindowHandleTypeXlib,
    WindowHandleTypeXcb,
    WindowHandleTypeWayland,
    WindowHandleTypeAndroid,
    WindowHandleTypeViNn,
} WindowHandleType;

typedef struct WindowHandle
{
    WindowHandleType type;

#if defined(_WIN32)
    HWND window;
#elif defined(ANDROID)
    ANativeWindow*   window;
    ANativeActivity* activity;
    AConfiguration*  configuration;
#elif defined(__APPLE__) || defined(NX64) || defined(ORBIS) || defined(PROSPERO)
    void*   window;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t*        connection;
    xcb_window_t             window;
    xcb_screen_t*            screen;
    xcb_intern_atom_reply_t* atom_wm_delete_window;
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    union
    {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        struct
        {
            Display* display;
            Window   window;
            Atom     xlib_wm_delete_window;
            Colormap colormap;
        };
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        struct
        {
            struct wl_display* wl_display;
            struct wl_surface* wl_surface;
        };
#endif
    };
#endif
} WindowHandle;

typedef struct WindowDesc
{
    WindowHandle Handle;
    RectDesc     WindowedRect;
    RectDesc     FullscreenRect;
    RectDesc     ClientRect;
    uint32_t     Flags;
    bool         Fullscreen;
    bool         CursorCaptured;
    bool         Iconified;
    bool         Maximized;
    bool         Minimized;
    bool         Hidden;
    bool         NoResizeFrame;
    bool         Borderless;
    bool         OverrideDefaultPosition;
    bool         Centered;
    bool         ForceLowDpi;

    int32_t Mode;

    int32_t CurrentResolution[MAX_MONITOR_COUNT];
    int32_t LastResolution[MAX_MONITOR_COUNT];

    int32_t WindowX;
    int32_t WindowY;
    int32_t WindowWidth;
    int32_t WindowHeight;

    bool    CursorHidden;
    int32_t CursorInsideWindow;
    bool    CursorCaptureRequested;
    bool    MinimizeRequested;

#if WINDOW_DETAILS
    bstring pWindowedRectLabel;
    bstring pFullscreenRectLabel;
    bstring pClientRectLabel;
    bstring pWndLabel;

    bstring pFullscreenLabel;
    bstring pCursorCapturedLabel;
    bstring pIconifiedLabel;
    bstring pMaximizedLabel;
    bstring pMinimizedLabel;
    bstring pNoResizeFrameLabel;
    bstring pBorderlessWindowLabel;
    bstring pOverrideDefaultPositionLabel;
    bstring pCenteredLabel;
    bstring pForceLowDPILabel;
    bstring pWindowModeLabel;
#endif

} WindowDesc;

typedef struct Resolution
{
    uint32_t width;
    uint32_t height;
} Resolution;

// Monitor data
//
typedef struct
{
    RectDesc     MonitorRect;
    RectDesc     WorkRect;
    unsigned int Dpi[2];
    unsigned int PhysicalSize[2];
    // This Size matches the static Size of DISPLAY_DEVICE.DeviceName
#if defined(_WINDOWS) || defined(XBOX)
    WCHAR AdapterName[32];
    WCHAR DisplayName[32];
    WCHAR PublicAdapterName[128];
    WCHAR PublicDisplayName[128];
#elif defined(__APPLE__)
#if defined(TARGET_IOS)
#else
    CGDirectDisplayID displayID;
    char              PublicAdapterName[64];
    char              PublicDisplayName[64];
#endif
#elif defined(__linux__) && !defined(__ANDROID__)
    Screen* screen;
    char    AdapterName[32];
    char    DisplayName[32];
    char    PublicAdapterName[64];
    char    PublicDisplayName[64];
#else
    char                     AdapterName[32];
    char                     DisplayName[32];
    char                     PublicAdapterName[64];
    char                     PublicDisplayName[64];
#endif
    // stb_ds array of Resolutions
    Resolution* Resolutions;
    Resolution  DefaultResolution;
    bool        ModesPruned;
    bool        ModeChanged;
} MonitorDesc;

typedef enum ThermalStatus
{
    ThermalStatusMin = -2,

    ThermalStatusNotSupported = -2, // Current platform/device/build doesn't support ThermalStatus queries
    ThermalStatusError = -1,         // There was an error retrieving thermal status

    // Note: These enum names are taken from Android, we might want to rename them or create our own
    ThermalStatusNone = 0,     // Means we didn't get any thermal status data yet (maybe device doesn't support it)
    ThermalStatusLight = 1,    // iOS Nominal
    ThermalStatusModerate = 2, // iOS Fair
    ThermalStatusSevere = 3,   // iOS Serious
    ThermalStatusCritical = 4, // iOS Critical
    ThermalStatusEmergency = 5,
    ThermalStatusShutdown = 6,

    ThermalStatusMax,
} ThermalStatus;

inline int GetRectWidth(const RectDesc* rect) { return rect->right - rect->left; }
inline int GetRectHeight(const RectDesc* rect) { return rect->bottom - rect->top; }

// Window handling
FORGE_API void OpenWindow(const char* app_name, WindowDesc* winDesc);
FORGE_API void ClosePlatformWindow(WindowDesc* winDesc);
FORGE_API void SetWindowRect(WindowDesc* winDesc, const RectDesc* rect);
FORGE_API void SetWindowSize(WindowDesc* winDesc, unsigned width, unsigned height);
FORGE_API void SetWindowed(WindowDesc* winDesc, unsigned width, unsigned height);
FORGE_API void SetBorderless(WindowDesc* winDesc, unsigned width, unsigned height);
FORGE_API void SetFullscreen(WindowDesc* winDesc);
FORGE_API void MakeWindowVisible(WindowDesc* winDesc);
FORGE_API void HideWindow(WindowDesc* winDesc);
FORGE_API void MaximizeWindow(WindowDesc* winDesc);
FORGE_API void MinimizeWindow(WindowDesc* winDesc);
FORGE_API void CenterWindow(WindowDesc* winDesc);
FORGE_API void CaptureCursor(WindowDesc* winDesc, bool bEnable);

// Mouse and cursor handling
FORGE_API void* CreateCursorFromFile(const char* path);
FORGE_API void  SetActiveCursor(void* cursor);
FORGE_API void  ShowMouseCursor(void);
FORGE_API void  HideMouseCursor(void);
FORGE_API bool  IsCursorInsideTrackingArea(void);
FORGE_API void  SetMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y);
FORGE_API void  SetMousePositionAbsolute(int32_t x, int32_t y);

FORGE_API void GetRecommendedResolution(RectDesc* rect);
FORGE_API void GetRecommendedWindowRect(WindowDesc* winDesc, RectDesc* rect);
// Sets video mode for specified display
FORGE_API void SetResolution(const MonitorDesc* pMonitor, const Resolution* pRes);

FORGE_API MonitorDesc* GetMonitor(uint32_t index);
FORGE_API uint32_t     GetMonitorCount(void);
FORGE_API uint32_t     GetActiveMonitorIndex(void);
// pArray pointer to array with at least 2 elements(x,y)
FORGE_API void         GetMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2]);

FORGE_API bool IsResolutionSupported(const MonitorDesc* pMonitor, const Resolution* pRes);

FORGE_API ThermalStatus GetThermalStatus(void);

inline const char* getThermalStatusString(ThermalStatus thermalStatus)
{
    switch (thermalStatus)
    {
    case ThermalStatusNotSupported:
        return "NotSupported";
    case ThermalStatusError:
        return "Error";
    case ThermalStatusNone:
        return "None";
    case ThermalStatusLight:
        return "Light";
    case ThermalStatusModerate:
        return "Moderate";
    case ThermalStatusSevere:
        return "Severe";
    case ThermalStatusCritical:
        return "Critical";
    case ThermalStatusEmergency:
        return "Emergency";
    case ThermalStatusShutdown:
        return "Shutdown";
    default:
        return "Invalid";
    }
}

// Reset
#if defined(_WINDOWS) || defined(__ANDROID__)
typedef enum ResetType
{
    ResetTypeNone = 0,
    ResetTypeApiSwitch = 0x1,
    ResetTypeGraphicCardSwitch = 0x2,
#if defined(_WINDOWS)
    ResetTypeDeviceLost = 0x4,
#endif
} ResetType;

typedef struct ResetDesc
{
    ResetType type;
} ResetDesc;

FORGE_API void RequestReset(const ResetDesc* pResetDesc);
#endif

// Reload
typedef enum ReloadType
{
    RELOAD_TYPE_RESIZE = 0x1,
    RELOAD_TYPE_SHADER = 0x2,
    RELOAD_TYPE_RENDERTARGET = 0x4,
    RELOAD_TYPE_ALL = UINT32_MAX,
    RELOAD_TYPE_COUNT = 3,
} ReloadType;
COMPILE_ASSERT(RELOAD_TYPE_COUNT == 3);

typedef struct ReloadDesc
{
    ReloadType type;
} ReloadDesc;

#if defined(ANDROID)
#define TF_MEMORY_STATE_API
typedef enum MemoryState
{
    MEMORY_STATE_UNKNOWN,
    MEMORY_STATE_OK,
    MEMORY_STATE_APPROACHING_LIMIT,
    MEMORY_STATE_CRITICAL,
} MemoryState;

inline const char* getMemoryStateString(MemoryState state)
{
    switch (state)
    {
    case MEMORY_STATE_UNKNOWN:
        return "Unknown";
    case MEMORY_STATE_OK:
        return "Okay";
    case MEMORY_STATE_APPROACHING_LIMIT:
        return "Approaching Limit";
    case MEMORY_STATE_CRITICAL:
        return "Critical";
    default:
        return "Invalid";
    }
}

FORGE_API MemoryState GetMemoryState(void);
#endif

typedef struct OSInfo
{
    char osName[256];
    char osVersion[256];
    char osDeviceName[256];
} OSInfo;

FORGE_API OSInfo* GetOsInfo(void);

//------------------------------------------------------------------------
// PLATFORM LAYER
//------------------------------------------------------------------------

FORGE_API void RequestReload(const ReloadDesc* pReloadDesc);

// API functions
FORGE_API void RequestShutdown(void);
typedef void (*errorMessagePopupCallbackFn)(void);
FORGE_API void ErrorMessagePopup(const char* title, const char* msg, WindowHandle* windowHandle, errorMessagePopupCallbackFn callback);

// Custom processing of OS pipe messages
typedef int32_t (*CustomMessageProcessor)(WindowDesc* pWindow, void* msg);
FORGE_API void SetCustomMessageProcessor(CustomMessageProcessor proc);

// Shell commands

#ifdef __cplusplus
extern "C"
{
#endif
    /// @param stdOutFile The file to which the output of the command should be written. May be NULL.
    FORGE_API int SystemRun(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile);
#ifdef __cplusplus
}
#endif
//
// failure research ...
//
