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

#include "Core/IConfig.h"

#include <ThirdParty/stb/stb_ds.h>
#include <ThirdParty/bstrlib/bstrlib.h>

#include "Application/IApp.h"
#include "Platform/IOperatingSystem.h"
#include "Core/ILog.h"

#include "Core/IMath.h"
#include "Core/IAlgorithm.h"

#pragma comment(lib, "WinMM.lib")
#include <windowsx.h>

#define elementsOf(a)      (sizeof(a) / sizeof((a)[0]))

#define LEFTEXTENDWIDTH    6
#define RIGHTEXTENDWIDTH   6
#define BOTTOMEXTENDWIDTH  20
#define TOPEXTENDWIDTH     27

#define FORGE_WINDOW_CLASS L"The Forge"

IApp* pWindowAppRef = NULL;

WindowDesc* gWindow = nullptr;
bool        gWindowIsResizing = false;

bool      gWindowClassInitialized = false;
WNDCLASSW gWindowClass;

bool gCursorVisible = true;
bool gCursorInsideRectangle = false;

MonitorDesc* gMonitors = nullptr;
uint32_t     gMonitorCount = 0;

// WindowsBase.cpp
extern CustomMessageProcessor sCustomProc;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

struct MonitorInfo
{
    unsigned index;
    WCHAR    AdapterName[32];
};

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

RECT         getCenteredWindowRect(WindowDesc* winDesc);
static DWORD PrepareStyleMask(WindowDesc* winDesc);

static void UpdateWindowDescFullScreenRect(WindowDesc* winDesc)
{
    HMONITOR      currentMonitor = MonitorFromWindow((HWND)winDesc->Handle.window, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX info;
    info.cbSize = sizeof(MONITORINFOEX);
    bool infoRead = GetMonitorInfo(currentMonitor, &info);
    ASSERT(infoRead);

    winDesc->FullscreenRect.left = info.rcMonitor.left;
    winDesc->FullscreenRect.top = info.rcMonitor.top;
    winDesc->FullscreenRect.right = info.rcMonitor.right;
    winDesc->FullscreenRect.bottom = info.rcMonitor.bottom;
}

static void UpdateWindowDescWindowedRect(WindowDesc* winDesc)
{
    RECT WindowedRect;
    HWND hwnd = (HWND)winDesc->Handle.window;

    GetWindowRect(hwnd, &WindowedRect);

    if (WindowedRect.left != winDesc->WindowedRect.left || WindowedRect.right != winDesc->WindowedRect.right ||
        WindowedRect.top != winDesc->WindowedRect.top || WindowedRect.bottom != winDesc->WindowedRect.bottom)
    {
        winDesc->WindowedRect.left = max(WindowedRect.left, 0l);
        winDesc->WindowedRect.right = WindowedRect.right;
        winDesc->WindowedRect.top = max(WindowedRect.top, 0l);
        winDesc->WindowedRect.bottom = WindowedRect.bottom;

        if (winDesc->Handle.window != NULL && !winDesc->Fullscreen)
        {
            RECT centeredRect = getCenteredWindowRect(winDesc);
            if (!winDesc->Borderless)
            {
                AdjustWindowRectExForDpi(&centeredRect, PrepareStyleMask(winDesc), false, 0, GetDpiForWindow((HWND)winDesc->Handle.window));
            }
            winDesc->Centered = WindowedRect.left == centeredRect.left && WindowedRect.right == centeredRect.right &&
                                WindowedRect.top == centeredRect.top && WindowedRect.bottom == centeredRect.bottom;
        }
        winDesc->WindowX = WindowedRect.left;
        winDesc->WindowY = WindowedRect.top;
        winDesc->WindowWidth = GetRectWidth(&winDesc->WindowedRect);
        winDesc->WindowHeight = GetRectHeight(&winDesc->WindowedRect);
    }
}

static void UpdateWindowDescClientRect(WindowDesc* winDesc)
{
    RECT ClientRect;
    HWND hwnd = (HWND)winDesc->Handle.window;

    GetClientRect(hwnd, &ClientRect);

    winDesc->ClientRect.left = ClientRect.left;
    winDesc->ClientRect.right = ClientRect.right;
    winDesc->ClientRect.top = ClientRect.top;
    winDesc->ClientRect.bottom = ClientRect.bottom;
}

static DWORD PrepareStyleMask(WindowDesc* winDesc)
{
    DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    if (winDesc->Borderless)
    {
        windowStyle = WS_POPUP | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    }

    if (winDesc->NoResizeFrame)
    {
        windowStyle ^= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    if (!winDesc->Hidden)
    {
        windowStyle |= WS_VISIBLE;
    }

    return windowStyle;
}

static void OffsetRectToDisplay(WindowDesc* winDesc, LPRECT rect)
{
    int32_t displayOffsetX = winDesc->FullscreenRect.left;
    int32_t displayOffsetY = winDesc->FullscreenRect.top;

    // Adjust for display coordinates in absolute virtual
    // display space.
    rect->left += (LONG)displayOffsetX;
    rect->top += (LONG)displayOffsetY;
    rect->right += (LONG)displayOffsetX;
    rect->bottom += (LONG)displayOffsetY;
}

void onResize(WindowDesc* wnd, int32_t newSizeX, int32_t newSizeY, bool Maximized)
{
    if (pWindowAppRef == nullptr || !pWindowAppRef->settings.initialized)
    {
        return;
    }

    pWindowAppRef->settings.Fullscreen = wnd->Fullscreen;
    if (pWindowAppRef->settings.Width == newSizeX && pWindowAppRef->settings.Height == newSizeY)
    {
        return;
    }

    pWindowAppRef->settings.Width = newSizeX;
    pWindowAppRef->settings.Height = newSizeY;
    wnd->Maximized = Maximized;
    wnd->Minimized = false;

    // Adjust windowed rect for windowed mode rendering.
    RECT rect = { 0L, 0L, (LONG)newSizeX, (LONG)newSizeY };

    if (!wnd->Borderless)
    {
        DWORD windowStyle = PrepareStyleMask(wnd);
        AdjustWindowRectExForDpi(&rect, windowStyle, FALSE, 0, GetDpiForWindow((HWND)wnd->Handle.window));
    }

    wnd->WindowWidth = rect.right - rect.left;
    wnd->WindowHeight = rect.bottom - rect.top;

    ReloadDesc reloadDesc;
    reloadDesc.Type = RELOAD_TYPE_RESIZE;
    RequestReload(&reloadDesc);
}

void adjustWindow(WindowDesc* winDesc)
{
    HWND hwnd = (HWND)winDesc->Handle.window;

    if (winDesc->Fullscreen)
    {
        RECT WindowedRect = {};
        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(hwnd, &WindowedRect);
        winDesc->WindowedRect = { (int)WindowedRect.left, (int)WindowedRect.top, (int)WindowedRect.right, (int)WindowedRect.bottom };

        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(hwnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);

        // Get the settings of the durrent display index. We want the app to go into
        // fullscreen mode on the display that supports Independent Flip.
        HMONITOR      currentMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEX info;
        info.cbSize = sizeof(MONITORINFOEX);
        bool infoRead = GetMonitorInfo(currentMonitor, &info);
        ASSERT(infoRead);

        pWindowAppRef->settings.windowX = info.rcMonitor.left;
        pWindowAppRef->settings.windowY = info.rcMonitor.top;

        SetWindowPos(hwnd, HWND_NOTOPMOST, info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left,
                     info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hwnd, SW_MAXIMIZE);

        onResize(winDesc, info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top, false);
    }
    else
    {
        DWORD windowStyle = PrepareStyleMask(winDesc);
        SetWindowLong(hwnd, GWL_STYLE, windowStyle);

        SetWindowPos(hwnd, HWND_NOTOPMOST, winDesc->WindowedRect.left, winDesc->WindowedRect.top, GetRectWidth(&winDesc->WindowedRect),
                     GetRectHeight(&winDesc->WindowedRect), SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

        if (winDesc->Maximized)
        {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
        else
        {
            ShowWindow(hwnd, SW_NORMAL);
        }
    }
}

static BOOL CALLBACK monitorCallback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
{
    UNREF_PARAM(pDeviceContext);
    UNREF_PARAM(pRect);
    MONITORINFOEXW info;
    info.cbSize = sizeof(info);
    GetMonitorInfoW(pMonitor, &info);
    MonitorInfo* data = (MonitorInfo*)pParam;
    unsigned     index = data->index;

    if (wcscmp(info.szDevice, data->AdapterName) == 0)
    {
        gMonitors[index].MonitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right,
                                         (int)info.rcMonitor.bottom };
        gMonitors[index].WorkRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
    }
    return TRUE;
}

// @Konstantin: This needs to be rethought, because this should happen
// very early. For Hades this happens too late (after init) which is
// unacceptable and causes issues.
void collectMonitorInfo()
{
    if (gMonitors != nullptr)
    {
        return;
    }

    DISPLAY_DEVICEW adapter;
    adapter.cb = sizeof(adapter);

    int      found = 0;
    uint32_t monitorCount = 0;

    for (int adapterIndex = 0;; ++adapterIndex)
    {
        if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
            break;

        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;

        for (int displayIndex = 0;; displayIndex++)
        {
            DISPLAY_DEVICEW display;
            display.cb = sizeof(display);

            if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                break;

            ++monitorCount;
        }
    }

    if (monitorCount)
    {
        gMonitorCount = monitorCount;
        gMonitors = (MonitorDesc*)MemCallocate(monitorCount, sizeof(MonitorDesc));
        for (int adapterIndex = 0;; ++adapterIndex)
        {
            if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
                break;

            if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;

            for (int displayIndex = 0;; displayIndex++)
            {
                DISPLAY_DEVICEW display;
                HDC             dc;

                display.cb = sizeof(display);

                if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                    break;

                dc = CreateDCW(L"DISPLAY", adapter.DeviceName, NULL, NULL);

                MonitorDesc desc;
                desc.ModesPruned = (adapter.StateFlags & DISPLAY_DEVICE_MODESPRUNED) != 0;

                wcsncpy_s(desc.AdapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));
                wcsncpy_s(desc.PublicAdapterName, adapter.DeviceString, elementsOf(adapter.DeviceString));
                wcsncpy_s(desc.DisplayName, display.DeviceName, elementsOf(display.DeviceName));
                wcsncpy_s(desc.PublicDisplayName, display.DeviceString, elementsOf(display.DeviceString));

                desc.PhysicalSize[0] = GetDeviceCaps(dc, HORZSIZE);
                desc.PhysicalSize[1] = GetDeviceCaps(dc, VERTSIZE);

                desc.Dpi[0] = (UINT)::GetDeviceCaps(dc, LOGPIXELSX);
                desc.Dpi[1] = (UINT)::GetDeviceCaps(dc, LOGPIXELSY);

                gMonitors[found] = (desc);
                MonitorInfo data = {};
                data.index = found;
                wcsncpy_s(data.AdapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));

                EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM)(&data));

                DeleteDC(dc);

                if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) && displayIndex == 0)
                {
                    MonitorDesc mDesc = gMonitors[0];
                    gMonitors[0] = gMonitors[found];
                    gMonitors[found] = mDesc;
                }

                found++;
            }
        }
    }
    else
    {
        LOGF(LogLevel::Debug, "FallBack Option");
        // Fallback options incase enumeration fails
        // then default to the primary device
        monitorCount = 0;
        HMONITOR currentMonitor = NULL;
        currentMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
        if (currentMonitor)
        {
            monitorCount = 1;
            gMonitors = (MonitorDesc*)MemCallocate(monitorCount, sizeof(MonitorDesc));

            MONITORINFOEXW info;
            info.cbSize = sizeof(MONITORINFOEXW);
            bool infoRead = GetMonitorInfoW(currentMonitor, &info);
            ASSERT(infoRead);
            MonitorDesc desc = {};

            wcsncpy_s(desc.AdapterName, info.szDevice, elementsOf(info.szDevice));
            wcsncpy_s(desc.PublicAdapterName, info.szDevice, elementsOf(info.szDevice));
            wcsncpy_s(desc.DisplayName, info.szDevice, elementsOf(info.szDevice));
            wcsncpy_s(desc.PublicDisplayName, info.szDevice, elementsOf(info.szDevice));
            desc.MonitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right, (int)info.rcMonitor.bottom };
            desc.WorkRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
            gMonitors[0] = (desc);
            gMonitorCount = monitorCount;
        }
    }

    for (uint32_t monitor = 0; monitor < monitorCount; ++monitor)
    {
        MonitorDesc* pMonitor = &gMonitors[monitor];
        DEVMODEW     devMode = {};
        devMode.dmSize = sizeof(DEVMODEW);
        devMode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;

        EnumDisplaySettingsW(pMonitor->AdapterName, ENUM_CURRENT_SETTINGS, &devMode);
        pMonitor->DefaultResolution.Height = devMode.dmPelsHeight;
        pMonitor->DefaultResolution.Width = devMode.dmPelsWidth;

        pMonitor->Dpi[0] = (uint32_t)((pMonitor->DefaultResolution.Width * 25.4) / pMonitor->PhysicalSize[0]);
        pMonitor->Dpi[1] = (uint32_t)((pMonitor->DefaultResolution.Height * 25.4) / pMonitor->PhysicalSize[1]);

        Resolution* Resolutions = NULL;
        DWORD       current = 0;

        arrsetcap(Resolutions, 8);

        while (EnumDisplaySettingsW(pMonitor->AdapterName, current++, &devMode))
        {
            bool duplicate = false;
            for (ptrdiff_t i = 0; i < arrlen(Resolutions); ++i)
            {
                if (Resolutions[i].Width == (uint32_t)devMode.dmPelsWidth && Resolutions[i].Height == (uint32_t)devMode.dmPelsHeight)
                {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate)
                continue;

            Resolution videoMode = {};
            videoMode.Height = devMode.dmPelsHeight;
            videoMode.Width = devMode.dmPelsWidth;
            arrpush(Resolutions, videoMode);
        }
        Sort(
            Resolutions, arrlenu(Resolutions), sizeof(Resolution),
            +[](const void* lhs, const void* rhs, void* User)
            {
                UNREF_PARAM(User);
                Resolution* pLhs = (Resolution*)lhs;
                Resolution* pRhs = (Resolution*)rhs;
                if (pLhs->Height == pRhs->Height)
                {
                    return pLhs->Width < pRhs->Width;
                }

                return pLhs->Height < pRhs->Height;
            },
            NULL);

        pMonitor->Resolutions = Resolutions;
    }
}

static void onFocusChanged(bool focused)
{
    if (pWindowAppRef == nullptr || !pWindowAppRef->settings.initialized)
    {
        return;
    }

    pWindowAppRef->settings.focused = focused;
}

// Hit test the frame for resizing and moving.
static LRESULT HitTestNCA(HWND hWnd, WPARAM /*wParam*/, LPARAM lParam)
{
    // Get the point coordinates for the hit test.
    POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    // Get the window rectangle.
    RECT rcWindow;
    GetWindowRect(hWnd, &rcWindow);

    // Get the frame rectangle, adjusted for the style without a caption.
    RECT rcFrame = { 0 };
    AdjustWindowRectEx(&rcFrame, WS_OVERLAPPEDWINDOW & ~WS_CAPTION, FALSE, NULL);

    // Determine if the hit test is for resizing. Default middle (1,1).
    USHORT uRow = 1;
    USHORT uCol = 1;
    bool   fOnResizeBorder = false;

    if (::IsZoomed(hWnd)) // if Maximized, only the frame title remains
    {
        // Determine if the point is at the top or bottom of the window.
        if ((ptMouse.y >= 0) && (ptMouse.y < TOPEXTENDWIDTH))
        {
            uRow = 0;
        }
    }
    else
    {
        // Determine if the point is at the top or bottom of the window.
        if (ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + TOPEXTENDWIDTH)
        {
            fOnResizeBorder = (ptMouse.y < (rcWindow.top - rcFrame.top));
            uRow = 0;
        }
        else if (ptMouse.y < rcWindow.bottom && ptMouse.y >= rcWindow.bottom - BOTTOMEXTENDWIDTH)
        {
            uRow = 2;
        }

        // Determine if the point is at the left or right of the window.
        if (ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + LEFTEXTENDWIDTH)
        {
            uCol = 0; // left side
        }
        else if (ptMouse.x < rcWindow.right && ptMouse.x >= rcWindow.right - RIGHTEXTENDWIDTH)
        {
            uCol = 2; // right side
        }
    }

    // Hit test (HTTOPLEFT, ... HTBOTTOMRIGHT)
    LRESULT hitTests[3][3] = {
        { fOnResizeBorder ? HTTOPLEFT : HTLEFT, fOnResizeBorder ? HTTOP : HTCAPTION, fOnResizeBorder ? HTTOPRIGHT : HTRIGHT },
        { HTLEFT, HTNOWHERE, HTRIGHT },
        { HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT },
    };

    return hitTests[uRow][uCol];
}

//------------------------------------------------------------------------
// WINDOW HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void OpenWindow(const char* app_name, WindowDesc* winDesc)
{
    UpdateWindowDescFullScreenRect(winDesc);

    // Defer borderless window setting
    bool borderless = winDesc->Borderless;
    winDesc->Borderless = false;

    // Adjust windowed rect for windowed mode rendering.
    RECT  rect = { (LONG)winDesc->ClientRect.left, (LONG)winDesc->ClientRect.top,
                   (LONG)winDesc->ClientRect.left + (LONG)winDesc->ClientRect.right,
                   (LONG)winDesc->ClientRect.top + (LONG)winDesc->ClientRect.bottom };
    DWORD windowStyle = PrepareStyleMask(winDesc);

    float dpiScale[2] = { 1.0f, 1.0f };
    GetMonitorDpiScale(pWindowAppRef ? (uint32_t)pWindowAppRef->settings.monitorIndex : 0, dpiScale);
    AdjustWindowRectExForDpi(&rect, windowStyle, FALSE, 0, (UINT)(dpiScale[0] * USER_DEFAULT_SCREEN_DPI));

    // Due to windows style and border, the rect can end up with negative values
    const int intMinPlusOne = INT_MIN + 1;
    if (rect.left < 0)
    {
        if (rect.left == INT_MIN)
            rect.left = intMinPlusOne;

        const int32_t offset = abs(rect.left);
        rect.left += offset;
        rect.right += offset;
    }

    if (rect.top < 0)
    {
        if (rect.top == INT_MIN)
            rect.top = intMinPlusOne;

        const int32_t offset = abs(rect.top);
        rect.top += offset;
        rect.bottom += offset;
    }

    WCHAR  app[FS_MAX_PATH] = {};
    size_t charConverted = 0;
    mbstowcs_s(&charConverted, app, app_name, FS_MAX_PATH);

    int windowY = rect.top;
    int windowX = rect.left;

    if (!winDesc->OverrideDefaultPosition)
    {
        windowX = windowY = CW_USEDEFAULT;
    }

    // Defer fullscreen. We always create in windowed, and
    // switch to fullscreen after creation.
    bool fullscreen = winDesc->Fullscreen;
    winDesc->Fullscreen = false;

    HWND hwnd = CreateWindowW(FORGE_WINDOW_CLASS, app, windowStyle, windowX, windowY, rect.right - windowX, rect.bottom - windowY, NULL,
                              NULL, (HINSTANCE)GetModuleHandle(NULL), 0);

    if (hwnd != NULL)
    {
        winDesc->Handle.Type = WindowHandleTypeWin32;
        winDesc->Handle.window = hwnd;

        GetClientRect(hwnd, &rect);
        winDesc->ClientRect = { (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom };

        GetWindowRect(hwnd, &rect);
        winDesc->WindowedRect = { (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom };

        if (!winDesc->Hidden)
        {
            if (winDesc->Maximized)
            {
                ShowWindow(hwnd, SW_MAXIMIZE);
            }
            else if (winDesc->Minimized)
            {
                ShowWindow(hwnd, SW_MINIMIZE);
            }

            if (borderless)
            {
                SetBorderless(winDesc, GetRectWidth(&winDesc->ClientRect), GetRectHeight(&winDesc->ClientRect));
            }

            if (winDesc->Centered)
            {
                CenterWindow(winDesc);
            }

            if (fullscreen)
            {
                SetFullscreen(winDesc);
            }
        }

        winDesc->WindowX = winDesc->WindowedRect.left;
        winDesc->WindowY = winDesc->WindowedRect.top;
        winDesc->WindowWidth = GetRectWidth(&winDesc->WindowedRect);
        winDesc->WindowHeight = GetRectHeight(&winDesc->WindowedRect);

        LOGF(LogLevel::Info, "Created window app %s", app_name);
    }
    else
    {
        LOGF(LogLevel::Error, "Failed to create window app %s", app_name);
    }

    SetMousePositionRelative(winDesc, GetRectWidth(&winDesc->WindowedRect) >> 1, GetRectHeight(&winDesc->WindowedRect) >> 1);
}

bool handleMessages()
{
    MSG msg;
    msg.message = NULL;
    bool quit = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (WM_QUIT == msg.message)
            quit = true;
    }

    return quit;
}

void ClosePlatformWindow(WindowDesc* winDesc)
{
    DestroyWindow((HWND)winDesc->Handle.window);
    winDesc->Handle = {};
    handleMessages();
}

void SetWindowRect(WindowDesc* winDesc, const RectDesc* rect)
{
    HWND hwnd = (HWND)winDesc->Handle.window;

    DWORD windowStyle = PrepareStyleMask(winDesc);
    SetWindowLong(hwnd, GWL_STYLE, windowStyle);

    winDesc->WindowedRect = *rect;

    if (winDesc->Centered)
    {
        CenterWindow(winDesc);
    }
    else
    {
        SetWindowPos(hwnd, HWND_NOTOPMOST, winDesc->WindowedRect.left, winDesc->WindowedRect.top,
                     winDesc->WindowedRect.right - winDesc->WindowedRect.left, winDesc->WindowedRect.bottom - winDesc->WindowedRect.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        RECT ClientRect;
        GetClientRect(hwnd, &ClientRect);
        winDesc->ClientRect.bottom = ClientRect.bottom;
        winDesc->ClientRect.top = ClientRect.top;
        winDesc->ClientRect.left = ClientRect.left;
        winDesc->ClientRect.right = ClientRect.right;
    }
}

void SetWindowSize(WindowDesc* winDesc, unsigned Width, unsigned Height)
{
    RectDesc newWindowRect;
    newWindowRect.left = winDesc->WindowedRect.left;
    newWindowRect.top = winDesc->WindowedRect.top;
    newWindowRect.right = newWindowRect.left + (int32_t)Width;
    newWindowRect.bottom = newWindowRect.top + (int32_t)Height;

    SetWindowRect(winDesc, &newWindowRect);
}

void toggleBorderless(WindowDesc* winDesc, unsigned clientWidth, unsigned clientHeight)
{
    if (!winDesc->Fullscreen)
    {
        winDesc->Borderless = !winDesc->Borderless;

        RECT newWindowRect;
        newWindowRect.left = winDesc->ClientRect.left;
        newWindowRect.top = winDesc->ClientRect.top;
        newWindowRect.right = newWindowRect.left + (int32_t)clientWidth;
        newWindowRect.bottom = newWindowRect.top + (int32_t)clientHeight;
        if (!winDesc->Borderless)
        {
            DWORD windowStyle = PrepareStyleMask(winDesc);
            AdjustWindowRectExForDpi(&newWindowRect, windowStyle, FALSE, 0, GetDpiForWindow((HWND)winDesc->Handle.window));
        }

        unsigned w = newWindowRect.right - newWindowRect.left;
        unsigned h = newWindowRect.bottom - newWindowRect.top;

        bool Centered = winDesc->Centered;
        winDesc->Centered = false;
        SetWindowSize(winDesc, w, h);
        winDesc->Centered = Centered;

        winDesc->WindowWidth = w;
        winDesc->WindowHeight = h;
        winDesc->Mode = winDesc->Borderless ? WindowModeBorderless : WindowModeWindowed;
    }
}

void toggleFullscreen(WindowDesc* winDesc)
{
    winDesc->Fullscreen = !winDesc->Fullscreen;
    adjustWindow(winDesc);
    if (winDesc->Fullscreen)
    {
        winDesc->WindowX = 0;
        winDesc->WindowY = 0;
        winDesc->WindowWidth = GetRectWidth(&winDesc->FullscreenRect);
        winDesc->WindowHeight = GetRectHeight(&winDesc->FullscreenRect);
        winDesc->Mode = WindowModeFullscreen;
    }
    else
    {
        winDesc->WindowX = winDesc->WindowedRect.left;
        winDesc->WindowY = winDesc->WindowedRect.top;
        winDesc->WindowWidth = GetRectWidth(&winDesc->WindowedRect);
        winDesc->WindowHeight = GetRectHeight(&winDesc->WindowedRect);
        winDesc->Mode = winDesc->Borderless ? WindowModeBorderless : WindowModeWindowed;
    }
}

void SetWindowed(WindowDesc* winDesc, unsigned Width, unsigned Height)
{
    UNREF_PARAM(Width);
    UNREF_PARAM(Height);
    winDesc->Maximized = false;

    if (winDesc->Fullscreen)
    {
        toggleFullscreen(winDesc);
    }
    if (winDesc->Borderless)
    {
        toggleBorderless(winDesc, GetRectWidth(&winDesc->ClientRect), GetRectHeight(&winDesc->ClientRect));
    }
    winDesc->Mode = WindowMode::WindowModeWindowed;
}

void SetBorderless(WindowDesc* winDesc, unsigned Width, unsigned Height)
{
    if (winDesc->Fullscreen)
    {
        toggleFullscreen(winDesc);
        if (!winDesc->Borderless)
        {
            toggleBorderless(winDesc, Width, Height);
        }
        winDesc->Mode = WindowMode::WindowModeBorderless;
    }
    else if (!winDesc->Borderless)
    {
        winDesc->Mode = WindowMode::WindowModeBorderless;
        toggleBorderless(winDesc, Width, Height);
        if (!winDesc->Borderless)
        {
            winDesc->Mode = WindowMode::WindowModeWindowed;
        }
    }
}

void SetFullscreen(WindowDesc* winDesc)
{
    if (!winDesc->Fullscreen)
    {
        toggleFullscreen(winDesc);
        winDesc->Mode = WindowMode::WindowModeFullscreen;
    }
}

void MakeWindowVisible(WindowDesc* winDesc)
{
    winDesc->Hidden = false;
    ShowWindow((HWND)winDesc->Handle.window, SW_SHOW);
}

void HideWindow(WindowDesc* winDesc)
{
    winDesc->Hidden = true;
    ShowWindow((HWND)winDesc->Handle.window, SW_HIDE);
}

void MaximizeWindow(WindowDesc* winDesc)
{
    winDesc->Maximized = true;
    ShowWindow((HWND)winDesc->Handle.window, SW_MAXIMIZE);
}

void MinimizeWindow(WindowDesc* winDesc)
{
    winDesc->Maximized = false;
    ShowWindow((HWND)winDesc->Handle.window, SW_MINIMIZE);
}

RECT getCenteredWindowRect(WindowDesc* winDesc)
{
    UpdateWindowDescFullScreenRect(winDesc);

    uint32_t FullscreenHalfWidth = GetRectWidth(&winDesc->FullscreenRect) >> 1;
    uint32_t FullscreenHalfHeight = GetRectHeight(&winDesc->FullscreenRect) >> 1;
    uint32_t windowWidth = GetRectWidth(&winDesc->ClientRect);
    uint32_t windowHeight = GetRectHeight(&winDesc->ClientRect);
    uint32_t windowHalfWidth = windowWidth >> 1;
    uint32_t windowHalfHeight = windowHeight >> 1;

    uint32_t X = FullscreenHalfWidth - windowHalfWidth;
    uint32_t Y = FullscreenHalfHeight - windowHalfHeight;

    RECT rect = { (LONG)(X), (LONG)(Y), (LONG)(X + windowWidth), (LONG)(Y + windowHeight) };
    return rect;
}

void CenterWindow(WindowDesc* winDesc)
{
    RECT rect = getCenteredWindowRect(winDesc);

    DWORD windowStyle = PrepareStyleMask(winDesc);

    AdjustWindowRectExForDpi(&rect, windowStyle, FALSE, 0, GetDpiForWindow((HWND)winDesc->Handle.window));

    OffsetRectToDisplay(winDesc, &rect);

    SetWindowPos((HWND)winDesc->Handle.window, HWND_NOTOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    winDesc->WindowedRect = { (int32_t)rect.left, (int32_t)rect.top, (int32_t)rect.right, (int32_t)rect.bottom };
}

//------------------------------------------------------------------------
// CURSOR AND MOUSE HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void* CreateCursorFromFile(const char* path) { return LoadCursorFromFileA(path); }

void SetActiveCursor(void* cursor)
{
    HCURSOR windowsCursor = (HCURSOR)cursor;
    SetCursor(windowsCursor);
}

void ShowMouseCursor()
{
    if (!gCursorVisible)
    {
        ShowCursor(TRUE);
        gCursorVisible = true;
    }
}

void HideMouseCursor()
{
    if (gCursorVisible)
    {
        ShowCursor(FALSE);
        gCursorVisible = false;
    }
}

void CaptureCursor(WindowDesc* winDesc, bool bEnable)
{
    ASSERT(winDesc);

    static int32_t lastCursorPosX = 0;
    static int32_t lastCursorPosY = 0;

    if (winDesc->CursorCaptured != bEnable)
    {
        if (bEnable)
        {
            POINT lastCursorPoint;
            GetCursorPos(&lastCursorPoint);
            lastCursorPosX = lastCursorPoint.x;
            lastCursorPosY = lastCursorPoint.y;

            HWND Handle = (HWND)winDesc->Handle.window;
            SetCapture(Handle);

            RECT ClientRect;
            GetClientRect(Handle, &ClientRect);
            // convert screen rect to client coordinates.
            POINT ptClientUL = { ClientRect.left, ClientRect.top };
            // Add one to the right and bottom sides, because the
            // coordinates retrieved by GetClientRect do not
            // include the far left and lowermost pixels.
            POINT ptClientLR = { ClientRect.right + 1, ClientRect.bottom + 1 };
            ClientToScreen(Handle, &ptClientUL);
            ClientToScreen(Handle, &ptClientLR);

            // Copy the client coordinates of the client area
            // to the rcClient structure. Confine the mouse cursor
            // to the client area by passing the rcClient structure
            // to the ClipCursor function.
            SetRect(&ClientRect, ptClientUL.x, ptClientUL.y, ptClientLR.x, ptClientLR.y);
            ClipCursor(&ClientRect);
            ShowCursor(FALSE);
        }
        else
        {
            ClipCursor(NULL);
            ShowCursor(TRUE);
            ReleaseCapture();
            SetCursorPos(lastCursorPosX, lastCursorPosY);
        }

        winDesc->CursorCaptured = bEnable;
    }
}

bool IsCursorInsideTrackingArea() { return gCursorInsideRectangle; }

void SetMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
    POINT point = { (LONG)x, (LONG)y };
    ClientToScreen((HWND)winDesc->Handle.window, &point);

    SetCursorPos(point.x, point.y);
}

void SetMousePositionAbsolute(int32_t x, int32_t y) { SetCursorPos(x, y); }

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void GetRecommendedResolution(RectDesc* rect)
{
    const int desiredWidth = 1920;
    const int desiredHeight = 1080;

    int maxWidth = (int)(GetSystemMetrics(SM_CXSCREEN) * 0.75);
    int maxHeight = (int)(GetSystemMetrics(SM_CYSCREEN) * 0.75);

    *rect = { 0, 0, min(desiredWidth, maxWidth), min(desiredHeight, maxHeight) };
}

void GetRecommendedWindowRect(WindowDesc* winDesc, RectDesc* rect)
{
    GetRecommendedResolution(rect);

    if (winDesc->Borderless == false && winDesc->Fullscreen == false)
    {
        RECT winRect;
        winRect.top = rect->top;
        winRect.bottom = rect->bottom;
        winRect.left = rect->left;
        winRect.right = rect->right;

        DWORD windowStyle = PrepareStyleMask(winDesc);
        AdjustWindowRectExForDpi(&winRect, windowStyle, FALSE, 0, GetDpiForWindow((HWND)winDesc->Handle.window));

        if (winRect.top < 0)
        {
            winRect.bottom -= winRect.top;
            winRect.top -= winRect.top;
        }
        if (winRect.left < 0)
        {
            winRect.right -= winRect.left;
            winRect.left -= winRect.left;
        }

        rect->top = winRect.top;
        rect->bottom = winRect.bottom;
        rect->left = winRect.left;
        rect->right = winRect.right;
    }
}

void SetResolution(const MonitorDesc* pMonitor, const Resolution* pMode)
{
    DEVMODEW devMode = {};
    devMode.dmSize = sizeof(DEVMODEW);
    devMode.dmPelsHeight = pMode->Height;
    devMode.dmPelsWidth = pMode->Width;
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    ChangeDisplaySettingsExW(pMonitor->AdapterName, &devMode, NULL, CDS_FULLSCREEN, NULL);
}

void getDpiScale(float array[2])
{
    HDC hdc = ::GetDC(NULL);
    array[0] = 0.f;
    array[1] = 0.f;
    const float Dpi = 96.0f;
    if (hdc)
    {
        array[0] = (UINT)(::GetDeviceCaps(hdc, LOGPIXELSX)) / Dpi;
        array[1] = (UINT)::GetDeviceCaps(hdc, LOGPIXELSY) / Dpi;
        ::ReleaseDC(NULL, hdc);
    }
    else
    {
        typedef UINT (*GetDpiForSystemFn)();
        static const GetDpiForSystemFn getDpiForSystem =
            (GetDpiForSystemFn)GetProcAddress(GetModuleHandle(TEXT("User32.dll")), "GetDpiForSystem");

        float dpiScale;
        if (getDpiForSystem)
            dpiScale = getDpiForSystem() / 96.0f;
        else
            dpiScale = 1.0f;

        array[0] = dpiScale;
        array[1] = dpiScale;
    }
}

void GetMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2])
{
    MonitorDesc* monitor = GetMonitor(monitorIndex);
    RECT         rect = { monitor->MonitorRect.left, monitor->MonitorRect.top, monitor->MonitorRect.right, monitor->MonitorRect.bottom };

    HMONITOR currentMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);

    typedef HRESULT (*GetDpiForMonitorFn)(HMONITOR hmonitor, int32_t dpiType, uint32_t * dpiX, uint32_t * dpiY);
    static const GetDpiForMonitorFn getDpiForMonitor =
        (GetDpiForMonitorFn)GetProcAddress(GetModuleHandle(TEXT("shcore.dll")), "GetDpiForMonitor");

    if (currentMonitor && getDpiForMonitor)
    {
        uint32_t dpiX = 0;
        uint32_t dpiY = 0;
        if (SUCCEEDED(getDpiForMonitor(currentMonitor, 0, &dpiX, &dpiY)))
        {
            dpiScale[0] = (float)dpiX / 96.0f;
            dpiScale[1] = (float)dpiY / 96.0f;
        }
    }
    else
    {
        getDpiScale(dpiScale);
    }

    LOGF(LogLevel::Debug, "Monitor index [%u] DPI scale = (%.2f, %.2f)", monitorIndex, dpiScale[0], dpiScale[1]);
}

bool IsResolutionSupported(const MonitorDesc* pMonitor, const Resolution* pRes)
{
    for (ptrdiff_t i = 0; i < arrlen(pMonitor->Resolutions); ++i)
    {
        if (pMonitor->Resolutions[i].Width == pRes->Width && pMonitor->Resolutions[i].Height == pRes->Height)
        {
            return true;
        }
    }

    return false;
}

MonitorDesc* GetMonitor(uint32_t index)
{
    ASSERT(gMonitorCount > index);
    return &gMonitors[index];
}

uint32_t GetMonitorCount() { return gMonitorCount; }

uint32_t GetActiveMonitorIndex()
{
    HMONITOR       currentMonitor = MonitorFromWindow((HWND)gWindow->Handle.window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(currentMonitor, &info);

    for (uint32_t i = 0; i < gMonitorCount; ++i)
    {
        if (wcscmp(info.szDevice, gMonitors[i].AdapterName) == 0)
        {
            return i;
        }
    }

    return 0;
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

// Window event handler - Use as less as possible
LRESULT CALLBACK WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (gWindow == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    if (gWindow->Borderless)
    {
        switch (message)
        {
        case WM_NCHITTEST:
        {
            auto lr = HitTestNCA(hwnd, wParam, lParam);
            if (lr == HTNOWHERE)
                return DefWindowProcW(hwnd, message, wParam, lParam);

            return lr;
        }
        case WM_NCCALCSIZE:
        {
            return 0;
        }
        }
    } // if (gWindow->Borderless)

    bool Maximized = false;
    switch (message)
    {
    case WM_NCPAINT:
    case WM_WINDOWPOSCHANGED:
    case WM_STYLECHANGED:
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_DPICHANGED:
    {
        if (!gWindow->Fullscreen)
        {
            const RECT& rect = *(const RECT*)lParam;
            SetWindowPos(hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_DISPLAYCHANGE:
    {
        adjustWindow(gWindow);
        break;
    }
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;

        // Prevent window from collapsing
        if (!gWindow->Fullscreen)
        {
            LONG zoomOffset = 128;
            lpMMI->ptMinTrackSize.x = zoomOffset;
            lpMMI->ptMinTrackSize.y = zoomOffset;
        }
        break;
    }
    case WM_ERASEBKGND:
    {
        // Make sure to keep consistent background color when resizing.
        HDC    hdc = (HDC)wParam;
        RECT   rc;
        HBRUSH hbrWhite = CreateSolidBrush(0x00000000);
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, hbrWhite);
        break;
    }
    case WM_WINDOWPOSCHANGING:
    case WM_MOVE:
    {
        UpdateWindowDescFullScreenRect(gWindow);
        if (!gWindow->Fullscreen)
        {
            UpdateWindowDescWindowedRect(gWindow);
        }
        break;
    }
    case WM_STYLECHANGING:
    {
        break;
    }
    case WM_SIZE:
    {
        switch (wParam)
        {
        case SIZE_MAXIMIZED:
            Maximized = true;
            onFocusChanged(true);
            gWindow->Minimized = false;
            if (!gWindow->Fullscreen && !gWindowIsResizing)
            {
                UpdateWindowDescClientRect(gWindow);
                onResize(gWindow, GetRectWidth(&gWindow->ClientRect), GetRectHeight(&gWindow->ClientRect), Maximized);
            }
            break;
        case SIZE_RESTORED:
            onFocusChanged(true);
            gWindow->Minimized = false;
            if (!gWindow->Fullscreen && !gWindowIsResizing)
            {
                UpdateWindowDescClientRect(gWindow);
                onResize(gWindow, GetRectWidth(&gWindow->ClientRect), GetRectHeight(&gWindow->ClientRect), Maximized);
            }
            break;
        case SIZE_MINIMIZED:
            onFocusChanged(false);
            gWindow->Minimized = true;
            break;
        }
        break;
    }
    case WM_SETFOCUS:
    {
        onFocusChanged(true);
        break;
    }
    case WM_KILLFOCUS:
    {
        onFocusChanged(false);
        break;
    }
    case WM_ENTERSIZEMOVE:
    {
        gWindowIsResizing = true;
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        onFocusChanged(true);
        gWindowIsResizing = false;
        if (!gWindow->Fullscreen)
        {
            UpdateWindowDescClientRect(gWindow);
            onResize(gWindow, GetRectWidth(&gWindow->ClientRect), GetRectHeight(&gWindow->ClientRect), false);
        }
        break;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT)
        {
            if (!gCursorInsideRectangle)
            {
                HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);
                SetCursor(cursor);

                gCursorInsideRectangle = true;
            }
        }
        else
        {
            gCursorInsideRectangle = false;
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
        break;
    }
    case WM_DESTROY:
    case WM_CLOSE:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_GETTEXT:
    {
        break;
    }
    default:
    {
        if (sCustomProc != nullptr)
        {
            MSG msg = {};
            msg.hwnd = hwnd;
            msg.lParam = lParam;
            msg.message = message;
            msg.wParam = wParam;

            sCustomProc(gWindow, &msg);
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    }
    return 0;
}

void initWindowClass()
{
    if (!gWindowClassInitialized)
    {
        HINSTANCE instance = (HINSTANCE)GetModuleHandle(NULL);
        memset(&gWindowClass, 0, sizeof(gWindowClass));
        gWindowClass.style = 0;
        gWindowClass.lpfnWndProc = WinProc;
        gWindowClass.hInstance = instance;
        gWindowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        gWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        gWindowClass.lpszClassName = FORGE_WINDOW_CLASS;

        bool success = RegisterClassW(&gWindowClass) != 0;

        if (!success)
        {
            // Get the error message, if any.
            DWORD errorMessageID = ::GetLastError();

            if (errorMessageID != ERROR_CLASS_ALREADY_EXISTS)
            {
                LPSTR messageBuffer = NULL;
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                               errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

                LOGF(Error, "%s", messageBuffer);
                LocalFree(messageBuffer);
                return;
            }
            else
            {
                gWindowClassInitialized = false;
            }
        }
    }

    collectMonitorInfo();
}

void exitWindowClass()
{
    for (uint32_t i = 0; i < gMonitorCount; ++i)
        arrfree(gMonitors[i].Resolutions);

    MemDeallocate(gMonitors);
    gMonitors = nullptr;
    gMonitorCount = 0;
}
