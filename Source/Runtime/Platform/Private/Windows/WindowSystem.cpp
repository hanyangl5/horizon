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
        uint32_t fsHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
        uint32_t fsHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
        uint32_t windowWidth = getRectWidth(&winDesc->clientRect);
        uint32_t windowHeight = getRectHeight(&winDesc->clientRect);
        uint32_t windowHalfWidth = windowWidth >> 1;
        uint32_t windowHalfHeight = windowHeight >> 1;

        int32_t X = fsHalfWidth - windowHalfWidth;
        int32_t Y = fsHalfHeight - windowHalfHeight;

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

void wndSetWindowed(void* pUserData)
{
    UNREF_PARAM(pUserData);
    setWindowed(pWindowRef, getRectWidth(&pWindowRef->clientRect), getRectHeight(&pWindowRef->clientRect));
}

void wndSetFullscreen(void* pUserData)
{
    UNREF_PARAM(pUserData);
    setFullscreen(pWindowRef);
}

void wndSetBorderless(void* pUserData)
{
    UNREF_PARAM(pUserData);
    setBorderless(pWindowRef, getRectWidth(&pWindowRef->clientRect), getRectHeight(&pWindowRef->clientRect));
}

void wndMaximizeWindow(void* pUserData)
{
    UNREF_PARAM(pUserData);
    WindowDesc* pWindow = pWindowRef;

    maximizeWindow(pWindow);
}

void wndMinimizeWindow(void* pUserData)
{
    UNREF_PARAM(pUserData);
    pWindowRef->minimizeRequested = true;
}

void wndHideWindow()
{
    WindowDesc* pWindow = pWindowRef;

    hideWindow(pWindow);
}

void wndShowWindow()
{
    WindowDesc* pWindow = pWindowRef;

    showWindow(pWindow);
}

void wndUpdateResolution(void* pUserData)
{
    UNREF_PARAM(pUserData);
    uint32_t monitorCount = getMonitorCount();
    for (uint32_t i = 0; i < monitorCount; ++i)
    {
        if (pWindowRef->pCurRes[i] != pWindowRef->pLastRes[i])
        {
            MonitorDesc* monitor = getMonitor(i);

            int32_t resIndex = pWindowRef->pCurRes[i];
            setResolution(monitor, &monitor->resolutions[resIndex]);

            pWindowRef->pLastRes[i] = pWindowRef->pCurRes[i];
        }
    }
}

void wndMoveWindow(void* pUserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(pUserData);
    int clientWidthStart = (getRectWidth(&pWindow->windowedRect) - getRectWidth(&pWindow->clientRect)) >> 1,
        clientHeightStart = getRectHeight(&pWindow->windowedRect) - getRectHeight(&pWindow->clientRect) - clientWidthStart;
    RectDesc rectDesc{ pWindowRef->wndX, pWindowRef->wndY, pWindowRef->wndX + pWindowRef->wndW, pWindowRef->wndY + pWindowRef->wndH };
    setWindowRect(pWindow, &rectDesc);
    LOGF(LogLevel::eINFO, "MoveWindow() Position check: %s",
         wndValidateWindowPos(pWindowRef->wndX + clientWidthStart, pWindowRef->wndY + clientHeightStart) ? "SUCCESS" : "FAIL");
    LOGF(LogLevel::eINFO, "MoveWindow() Size check: %s", wndValidateWindowSize(pWindowRef->wndW, pWindowRef->wndH) ? "SUCCESS" : "FAIL");
}

void wndSetRecommendedWindowSize(void* pUserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(pUserData);

    RectDesc rect;
    getRecommendedWindowRect(pWindowRef, &rect);

    setWindowRect(pWindow, &rect);

    pWindowRef->wndX = rect.left;
    pWindowRef->wndY = rect.top;
    pWindowRef->wndW = rect.right - rect.left;
    pWindowRef->wndH = rect.bottom - rect.top;
}

void wndHideCursor()
{
    pWindowRef->cursorHidden = true;
    hideCursor();
}

void wndShowCursor()
{
    pWindowRef->cursorHidden = false;
    showCursor();
}

void wndUpdateCaptureCursor(void* pUserData)
{
    UNREF_PARAM(pUserData);
#ifdef ENABLE_FORGE_INPUT
    setEnableCaptureInput(pWindowRef->cursorCaptureRequested);
#endif
}

#if defined(AUTOMATED_TESTING)
void wndTakeScreenshot(void* pUserData)
{
    UNREF_PARAM(pUserData);
    char screenShotName[256];
    snprintf(screenShotName, sizeof(screenShotName), "%s_%s", gAppName, gScriptNameBuffer);

    setCaptureScreenshot(screenShotName);
}
#endif

void platformInitWindowSystem(WindowDesc* pData)
{
    ASSERT(pWindowRef == NULL);

    RectDesc currentRes = pData->fullScreen ? pData->fullscreenRect : pData->windowedRect;
    pData->wndX = currentRes.left;
    pData->wndY = currentRes.top;
    pData->wndW = currentRes.right - currentRes.left;
    pData->wndH = currentRes.bottom - currentRes.top;

    pWindowRef = pData;

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
    pWindowRef->cursorInsideWindow = isCursorInsideTrackingArea();

    if (pWindowRef->minimizeRequested)
    {
        minimizeWindow(pWindowRef);
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
            pWindowRef->windowMode == WM_BORDERLESS ? "Borderless" : (pWindowRef->windowMode == WM_FULLSCREEN ? "Fullscreen" : "Windowed"));
#endif
}
