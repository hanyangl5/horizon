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
    int         clientWidthStart = (GetRectWidth(&winDesc->WindowedRect) - GetRectWidth(&winDesc->ClientRect)) >> 1;
    int         clientHeightStart = GetRectHeight(&winDesc->WindowedRect) - GetRectHeight(&winDesc->ClientRect) - clientWidthStart;

    if (winDesc->Centered)
    {
        uint32_t FullscreenHalfWidth = GetRectWidth(&winDesc->FullscreenRect) >> 1;
        uint32_t FullscreenHalfHeight = GetRectHeight(&winDesc->FullscreenRect) >> 1;
        uint32_t windowWidth = GetRectWidth(&winDesc->ClientRect);
        uint32_t windowHeight = GetRectHeight(&winDesc->ClientRect);
        uint32_t windowHalfWidth = windowWidth >> 1;
        uint32_t windowHalfHeight = windowHeight >> 1;

        int32_t X = FullscreenHalfWidth - windowHalfWidth;
        int32_t Y = FullscreenHalfHeight - windowHalfHeight;

        if ((abs(winDesc->WindowedRect.left + clientWidthStart - X) > 1) || (abs(winDesc->WindowedRect.top + clientHeightStart - Y) > 1))
        {
            return false;
        }
    }
    else
    {
        if ((abs(x - winDesc->WindowedRect.left - clientWidthStart) > 1) || (abs(y - winDesc->WindowedRect.top - clientHeightStart) > 1))
        {
            return false;
        }
    }

    return true;
}

static bool wndValidateWindowSize(int32_t Width, int32_t Height)
{
    RectDesc windowRect = pWindowRef->WindowedRect;
    if ((abs(GetRectWidth(&windowRect) - Width) > 1) || (abs(GetRectHeight(&windowRect) - Height) > 1))
    {
        return false;
    }
    return true;
}

void wndSetWindowed(void* UserData)
{
    UNREF_PARAM(UserData);
    SetWindowed(pWindowRef, GetRectWidth(&pWindowRef->ClientRect), GetRectHeight(&pWindowRef->ClientRect));
}

void wndSetFullscreen(void* UserData)
{
    UNREF_PARAM(UserData);
    SetFullscreen(pWindowRef);
}

void wndSetBorderless(void* UserData)
{
    UNREF_PARAM(UserData);
    SetBorderless(pWindowRef, GetRectWidth(&pWindowRef->ClientRect), GetRectHeight(&pWindowRef->ClientRect));
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
    pWindowRef->MinimizeRequested = true;
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
        if (pWindowRef->CurrentResolution[i] != pWindowRef->LastResolution[i])
        {
            MonitorDesc* monitor = GetMonitor(i);

            int32_t resIndex = pWindowRef->CurrentResolution[i];
            SetResolution(monitor, &monitor->Resolutions[resIndex]);

            pWindowRef->LastResolution[i] = pWindowRef->CurrentResolution[i];
        }
    }
}

void wndMoveWindow(void* UserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(UserData);
    int clientWidthStart = (GetRectWidth(&pWindow->WindowedRect) - GetRectWidth(&pWindow->ClientRect)) >> 1,
        clientHeightStart = GetRectHeight(&pWindow->WindowedRect) - GetRectHeight(&pWindow->ClientRect) - clientWidthStart;
    RectDesc rectDesc{ pWindowRef->WindowX, pWindowRef->WindowY, pWindowRef->WindowX + pWindowRef->WindowWidth,
                       pWindowRef->WindowY + pWindowRef->WindowHeight };
    SetWindowRect(pWindow, &rectDesc);
    LOGF(LogLevel::Info, "MoveWindow() Position check: %s",
         wndValidateWindowPos(pWindowRef->WindowX + clientWidthStart, pWindowRef->WindowY + clientHeightStart) ? "SUCCESS" : "FAIL");
    LOGF(LogLevel::Info, "MoveWindow() Size check: %s",
         wndValidateWindowSize(pWindowRef->WindowWidth, pWindowRef->WindowHeight) ? "SUCCESS" : "FAIL");
}

void wndSetRecommendedWindowSize(void* UserData)
{
    WindowDesc* pWindow = pWindowRef;

    wndSetWindowed(UserData);

    RectDesc rect;
    GetRecommendedWindowRect(pWindowRef, &rect);

    SetWindowRect(pWindow, &rect);

    pWindowRef->WindowX = rect.left;
    pWindowRef->WindowY = rect.top;
    pWindowRef->WindowWidth = rect.right - rect.left;
    pWindowRef->WindowHeight = rect.bottom - rect.top;
}

void wndHideCursor()
{
    pWindowRef->CursorHidden = true;
    HideMouseCursor();
}

void wndShowCursor()
{
    pWindowRef->CursorHidden = false;
    ShowMouseCursor();
}

void wndUpdateCaptureCursor(void* UserData)
{
    UNREF_PARAM(UserData);
#ifdef ENABLE_FORGE_INPUT
    SetEnableCaptureInput(pWindowRef->CursorCaptureRequested);
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

    RectDesc currentRes = Data->Fullscreen ? Data->FullscreenRect : Data->WindowedRect;
    Data->WindowX = currentRes.left;
    Data->WindowY = currentRes.top;
    Data->WindowWidth = currentRes.right - currentRes.left;
    Data->WindowHeight = currentRes.bottom - currentRes.top;

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
    pWindowRef->CursorInsideWindow = IsCursorInsideTrackingArea();

    if (pWindowRef->MinimizeRequested)
    {
        MinimizeWindow(pWindowRef);
        pWindowRef->MinimizeRequested = false;
    }

#if WINDOW_DETAILS
    bdestroy(&pWindowRef->pWindowedRectLabel);
    bformat(&pWindowRef->pWindowedRectLabel, "WindowedRect L: %d, T: %d, R: %d, B: %d", pWindowRef->WindowedRect.left,
            pWindowRef->WindowedRect.top, pWindowRef->WindowedRect.right, pWindowRef->WindowedRect.bottom);
    bdestroy(&pWindowRef->pFullscreenRectLabel);
    bformat(&pWindowRef->pFullscreenRectLabel, "FullscreenRect L: %d, T: %d, R: %d, B: %d", pWindowRef->FullscreenRect.left,
            pWindowRef->FullscreenRect.top, pWindowRef->FullscreenRect.right, pWindowRef->FullscreenRect.bottom);
    bdestroy(&pWindowRef->pClientRectLabel);
    bformat(&pWindowRef->pClientRectLabel, "ClientRect L: %d, T: %d, R: %d, B: %d", pWindowRef->ClientRect.left, pWindowRef->ClientRect.top,
            pWindowRef->ClientRect.right, pWindowRef->ClientRect.bottom);
    bdestroy(&pWindowRef->pWndLabel);
    bformat(&pWindowRef->pWndLabel, "Wnd X: %d, Y: %d, W: %d, H: %d", pWindowRef->WindowX, pWindowRef->WindowY, pWindowRef->WindowWidth,
            pWindowRef->WindowHeight);
    bdestroy(&pWindowRef->pFullscreenLabel);
    bformat(&pWindowRef->pFullscreenLabel, "Fullscreen: %s", pWindowRef->Fullscreen ? "True" : "False");
    bdestroy(&pWindowRef->pCursorCapturedLabel);
    bformat(&pWindowRef->pCursorCapturedLabel, "CursorCaptured: %s", pWindowRef->CursorCaptured ? "True" : "False");
    bdestroy(&pWindowRef->pIconifiedLabel);
    bformat(&pWindowRef->pIconifiedLabel, "Iconified: %s", pWindowRef->Iconified ? "True" : "False");
    bdestroy(&pWindowRef->pMaximizedLabel);
    bformat(&pWindowRef->pMaximizedLabel, "Maximized: %s", pWindowRef->Maximized ? "True" : "False");
    bdestroy(&pWindowRef->pMinimizedLabel);
    bformat(&pWindowRef->pMinimizedLabel, "Minimized: %s", pWindowRef->Minimized ? "True" : "False");
    bdestroy(&pWindowRef->pNoResizeFrameLabel);
    bformat(&pWindowRef->pNoResizeFrameLabel, "NoResizeFrame: %s", pWindowRef->NoResizeFrame ? "True" : "False");
    bdestroy(&pWindowRef->pBorderlessWindowLabel);
    bformat(&pWindowRef->pBorderlessWindowLabel, "BorderlessWindow: %s", pWindowRef->Borderless ? "True" : "False");
    bdestroy(&pWindowRef->pOverrideDefaultPositionLabel);
    bformat(&pWindowRef->pOverrideDefaultPositionLabel, "OverrideDefaultPosition: %s",
            pWindowRef->OverrideDefaultPosition ? "True" : "False");
    bdestroy(&pWindowRef->pCenteredLabel);
    bformat(&pWindowRef->pCenteredLabel, "Centered: %s", pWindowRef->Centered ? "True" : "False");
    bdestroy(&pWindowRef->pForceLowDPILabel);
    bformat(&pWindowRef->pForceLowDPILabel, "ForceLowDPI: %s", pWindowRef->ForceLowDpi ? "True" : "False");
    bdestroy(&pWindowRef->pWindowModeLabel);
    bformat(&pWindowRef->pWindowModeLabel, "WindowMode: %s",
            pWindowRef->Mode == WindowModeBorderless ? "Borderless"
                                                     : (pWindowRef->Mode == WindowModeFullscreen ? "Fullscreen" : "Windowed"));
#endif
}
