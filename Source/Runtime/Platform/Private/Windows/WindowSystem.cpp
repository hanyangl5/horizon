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

//#include "Application/IApp.h"
#include "Platform/IInput.h"
#include "Core/ILog.h"
//#include "Application/IScreenshot.h"
//#include "Application/IUI.h"
//#include <Scripting/IScripting.h>
#include "Platform/IOperatingSystem.h"
#include "Core/ITime.h"
#include <ThirdParty/bstrlib/bstrlib.h>

static WindowDesc* pWindowRef = NULL;

static char    gPlatformNameBuffer[64];
static bstring gPlatformName = bemptyfromarr(gPlatformNameBuffer);

#if defined(AUTOMATED_TESTING)
char           gAppName[64];
static char    gScriptNameBuffer[64];
static bstring gScriptName = bemptyfromarr(gScriptNameBuffer);
#endif

static bool wndValidateWindowPos(int32_t x, int32_t y)
{
    WindowDesc* winDesc = pWindowRef;
    int         clientWidthStart = (getRectWidth(&winDesc->windowedRect) - getRectWidth(&winDesc->clientRect)) >> 1;
    int         clientHeightStart = getRectHeight(&winDesc->windowedRect) - getRectHeight(&winDesc->clientRect) - clientWidthStart;

    if (winDesc->centered)
    {
        uint32_t FullscreenHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
        uint32_t FullscreenHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
        uint32_t windowWidth = getRectWidth(&winDesc->clientRect);
        uint32_t windowHeight = getRectHeight(&winDesc->clientRect);
        uint32_t windowHalfWidth = windowWidth >> 1;
        uint32_t windowHalfHeight = windowHeight >> 1;

        int32_t X = FullscreenHalfWidth - windowHalfWidth;
        int32_t Y = FullscreenHalfHeight - windowHalfHeight;

        if ((abs(winDesc->windowedRect.left + clientWidthStart - X) > 1) || (abs(winDesc->windowedRect.top + clientHeightStart - Y) > 1))
            return false;
    }
    else
    {
        if ((abs(x - winDesc->windowedRect.left - clientWidthStart) > 1) || (abs(y - winDesc->windowedRect.top - clientHeightStart) > 1))
            return false;
    }

    return true;
}

static bool wndValidateWindowSize(int32_t width, int32_t height)
{
    RectDesc windowRect = pWindowRef->windowedRect;
    if ((abs(getRectWidth(&windowRect) - width) > 1) || (abs(getRectHeight(&windowRect) - height) > 1))
        return false;
    return true;
}

void wndSetWindowed(void* UserData)
{
    UNREF_PARAM(UserData);
    SetWindowed(pWindowRef, getRectWidth(&pWindowRef->clientRect), getRectHeight(&pWindowRef->clientRect));
}

void wndSetFullscreen(void* UserData)
{
    UNREF_PARAM(UserData);
    SetFullscreen(pWindowRef);
}

void wndSetBorderless(void* UserData)
{
    UNREF_PARAM(UserData);
    SetBorderless(pWindowRef, getRectWidth(&pWindowRef->clientRect), getRectHeight(&pWindowRef->clientRect));
}

void wndMaximizeWindow(void* UserData)
{
    UNREF_PARAM(UserData);
    WindowDesc* pWindow = pWindowRef;

    MaximizeWindow(pWindow);
}

void wndMinimizeWindow(void* UserData)
{
    UNREF_PARAM(UserData);
    pWindowRef->minimizeRequested = true;
}

void wndHideWindow()
{
    WindowDesc* pWindow = pWindowRef;

    HideWindow(pWindow);
}

void wndShowWindow()
{
    WindowDesc* pWindow = pWindowRef;

    MakeWindowVisible(pWindow);
}

void wndUpdateResolution(void* UserData)
{
    UNREF_PARAM(UserData);
    uint32_t monitorCount = GetMonitorCount();
    for (uint32_t i = 0; i < monitorCount; ++i)
    {
        if (pWindowRef->pCurRes[i] != pWindowRef->pLastRes[i])
        {
            MonitorDesc* monitor = GetMonitor(i);

            int32_t resIndex = pWindowRef->pCurRes[i];
            SetResolution(monitor, &monitor->resolutions[resIndex]);

            pWindowRef->pLastRes[i] = pWindowRef->pCurRes[i];
        }
    }
}

void wndMoveWindow(void* UserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(UserData);
    int clientWidthStart = (getRectWidth(&pWindow->windowedRect) - getRectWidth(&pWindow->clientRect)) >> 1,
        clientHeightStart = getRectHeight(&pWindow->windowedRect) - getRectHeight(&pWindow->clientRect) - clientWidthStart;
    RectDesc rectDesc{ pWindowRef->wndX, pWindowRef->wndY, pWindowRef->wndX + pWindowRef->wndW, pWindowRef->wndY + pWindowRef->wndH };
    SetWindowRect(pWindow, &rectDesc);
    LOGF(LogLevel::Info, "MoveWindow() Position check: %s",
         wndValidateWindowPos(pWindowRef->wndX + clientWidthStart, pWindowRef->wndY + clientHeightStart) ? "SUCCESS" : "FAIL");
    LOGF(LogLevel::Info, "MoveWindow() Size check: %s", wndValidateWindowSize(pWindowRef->wndW, pWindowRef->wndH) ? "SUCCESS" : "FAIL");
}

void wndSetRecommendedWindowSize(void* UserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(UserData);

    RectDesc rect;
    GetRecommendedWindowRect(pWindowRef, &rect);

    SetWindowRect(pWindow, &rect);

    pWindowRef->wndX = rect.left;
    pWindowRef->wndY = rect.top;
    pWindowRef->wndW = rect.right - rect.left;
    pWindowRef->wndH = rect.bottom - rect.top;
}

void wndHideCursor()
{
    pWindowRef->cursorHidden = true;
    HideMouseCursor();
}

void wndShowCursor()
{
    pWindowRef->cursorHidden = false;
    ShowMouseCursor();
}

void wndUpdateCaptureCursor(void* UserData)
{
    UNREF_PARAM(UserData);
#ifdef ENABLE_FORGE_INPUT
    SetEnableCaptureInput(pWindowRef->cursorCaptureRequested);
#endif
}

#if defined(AUTOMATED_TESTING)
void wndTakeScreenshot(void* UserData)
{
    UNREF_PARAM(UserData);
    char screenShotName[256];
    snprintf(screenShotName, sizeof(screenShotName), "%s_%s", gAppName, gScriptNameBuffer);

    setCaptureScreenshot(screenShotName);
}
#endif

void platformInitWindowSystem(WindowDesc* Data)
{
    ASSERT(pWindowRef == NULL);

    RectDesc currentRes = Data->fullScreen ? Data->fullscreenRect : Data->windowedRect;
    Data->wndX = currentRes.left;
    Data->wndY = currentRes.top;
    Data->wndW = currentRes.right - currentRes.left;
    Data->wndH = currentRes.bottom - currentRes.top;

    pWindowRef = Data;

#if WINDOW_DETAILS
    pWindowRef->pWindowedRectLabel = bempty();
    pWindowRef->pFullscreenRectLabel = bempty();
    pWindowRef->pClientRectLabel = bempty();
    pWindowRef->pWndLabel = bempty();
    pWindowRef->pFullscreenLabel = bempty();
    pWindowRef->pCursorCapturedLabel = bempty();
    pWindowRef->pIconifiedLabel = bempty();
    pWindowRef->pMaximizedLabel = bempty();
    pWindowRef->pMinimizedLabel = bempty();
    pWindowRef->pNoResizeFrameLabel = bempty();
    pWindowRef->pBorderlessWindowLabel = bempty();
    pWindowRef->pOverrideDefaultPositionLabel = bempty();
    pWindowRef->pCenteredLabel = bempty();
    pWindowRef->pForceLowDPILabel = bempty();
    pWindowRef->pWindowModeLabel = bempty();
#endif
}

void platformExitWindowSystem()
{
#if WINDOW_DETAILS
    bdestroy(&pWindowRef->pWindowedRectLabel);
    bdestroy(&pWindowRef->pFullscreenRectLabel);
    bdestroy(&pWindowRef->pClientRectLabel);
    bdestroy(&pWindowRef->pWndLabel);
    bdestroy(&pWindowRef->pFullscreenLabel);
    bdestroy(&pWindowRef->pCursorCapturedLabel);
    bdestroy(&pWindowRef->pIconifiedLabel);
    bdestroy(&pWindowRef->pMaximizedLabel);
    bdestroy(&pWindowRef->pMinimizedLabel);
    bdestroy(&pWindowRef->pNoResizeFrameLabel);
    bdestroy(&pWindowRef->pBorderlessWindowLabel);
    bdestroy(&pWindowRef->pOverrideDefaultPositionLabel);
    bdestroy(&pWindowRef->pCenteredLabel);
    bdestroy(&pWindowRef->pForceLowDPILabel);
    bdestroy(&pWindowRef->pWindowModeLabel);
#endif
    pWindowRef = NULL;
}

void platformUpdateWindowSystem()
{
    pWindowRef->cursorInsideWindow = IsCursorInsideTrackingArea();

    if (pWindowRef->minimizeRequested)
    {
        MinimizeWindow(pWindowRef);
        pWindowRef->minimizeRequested = false;
    }

#if WINDOW_DETAILS
    bdestroy(&pWindowRef->pWindowedRectLabel);
    bformat(&pWindowRef->pWindowedRectLabel, "WindowedRect L: %d, T: %d, R: %d, B: %d", pWindowRef->windowedRect.left,
            pWindowRef->windowedRect.top, pWindowRef->windowedRect.right, pWindowRef->windowedRect.bottom);
    bdestroy(&pWindowRef->pFullscreenRectLabel);
    bformat(&pWindowRef->pFullscreenRectLabel, "FullscreenRect L: %d, T: %d, R: %d, B: %d", pWindowRef->fullscreenRect.left,
            pWindowRef->fullscreenRect.top, pWindowRef->fullscreenRect.right, pWindowRef->fullscreenRect.bottom);
    bdestroy(&pWindowRef->pClientRectLabel);
    bformat(&pWindowRef->pClientRectLabel, "ClientRect L: %d, T: %d, R: %d, B: %d", pWindowRef->clientRect.left, pWindowRef->clientRect.top,
            pWindowRef->clientRect.right, pWindowRef->clientRect.bottom);
    bdestroy(&pWindowRef->pWndLabel);
    bformat(&pWindowRef->pWndLabel, "Wnd X: %d, Y: %d, W: %d, H: %d", pWindowRef->wndX, pWindowRef->wndY, pWindowRef->wndW,
            pWindowRef->wndH);
    bdestroy(&pWindowRef->pFullscreenLabel);
    bformat(&pWindowRef->pFullscreenLabel, "Fullscreen: %s", pWindowRef->fullScreen ? "True" : "False");
    bdestroy(&pWindowRef->pCursorCapturedLabel);
    bformat(&pWindowRef->pCursorCapturedLabel, "CursorCaptured: %s", pWindowRef->cursorCaptured ? "True" : "False");
    bdestroy(&pWindowRef->pIconifiedLabel);
    bformat(&pWindowRef->pIconifiedLabel, "Iconified: %s", pWindowRef->iconified ? "True" : "False");
    bdestroy(&pWindowRef->pMaximizedLabel);
    bformat(&pWindowRef->pMaximizedLabel, "Maximized: %s", pWindowRef->maximized ? "True" : "False");
    bdestroy(&pWindowRef->pMinimizedLabel);
    bformat(&pWindowRef->pMinimizedLabel, "Minimized: %s", pWindowRef->minimized ? "True" : "False");
    bdestroy(&pWindowRef->pNoResizeFrameLabel);
    bformat(&pWindowRef->pNoResizeFrameLabel, "NoResizeFrame: %s", pWindowRef->noresizeFrame ? "True" : "False");
    bdestroy(&pWindowRef->pBorderlessWindowLabel);
    bformat(&pWindowRef->pBorderlessWindowLabel, "BorderlessWindow: %s", pWindowRef->borderlessWindow ? "True" : "False");
    bdestroy(&pWindowRef->pOverrideDefaultPositionLabel);
    bformat(&pWindowRef->pOverrideDefaultPositionLabel, "OverrideDefaultPosition: %s",
            pWindowRef->overrideDefaultPosition ? "True" : "False");
    bdestroy(&pWindowRef->pCenteredLabel);
    bformat(&pWindowRef->pCenteredLabel, "Centered: %s", pWindowRef->centered ? "True" : "False");
    bdestroy(&pWindowRef->pForceLowDPILabel);
    bformat(&pWindowRef->pForceLowDPILabel, "ForceLowDPI: %s", pWindowRef->forceLowDPI ? "True" : "False");
    bdestroy(&pWindowRef->pWindowModeLabel);
    bformat(&pWindowRef->pWindowModeLabel, "WindowMode: %s",
            pWindowRef->windowMode == WindowModeBorderless ? "Borderless" : (pWindowRef->windowMode == WindowModeFullscreen ? "Fullscreen" : "Windowed"));
#endif
}
