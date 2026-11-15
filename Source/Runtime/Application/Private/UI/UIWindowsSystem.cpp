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

#include "Application/IApp.h"
#include "Platform/IInput.h"
// #include "Application/IScreenshot.h"
#include "Application/IUI.h"
// #include <Scripting/IScripting.h>
#include "Core/ILog.h"
#include "Core/ITime.h"
#include "Platform/IOperatingSystem.h"

static WindowDesc*  pWindowRef = NULL;
static UIComponent* pWindowControlsComponent = NULL;

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
            return false;
    }
    else
    {
        if ((abs(x - winDesc->WindowedRect.left - clientWidthStart) > 1) || (abs(y - winDesc->WindowedRect.top - clientHeightStart) > 1))
            return false;
    }

    return true;
}

static bool wndValidateWindowSize(int32_t width, int32_t height)
{
    RectDesc windowRect = pWindowRef->WindowedRect;
    if ((abs(GetRectWidth(&windowRect) - width) > 1) || (abs(GetRectHeight(&windowRect) - height) > 1))
        return false;
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
    RectDesc rectDesc{ pWindowRef->WindowX, pWindowRef->WindowY, pWindowRef->WindowX + pWindowRef->WindowWidth, pWindowRef->WindowY + pWindowRef->WindowHeight };
    SetWindowRect(pWindow, &rectDesc);
    LOGF(LogLevel::Info, "MoveWindow() Position check: %s",
         wndValidateWindowPos(pWindowRef->WindowX + clientWidthStart, pWindowRef->WindowY + clientHeightStart) ? "SUCCESS" : "FAIL");
    LOGF(LogLevel::Info, "MoveWindow() Size check: %s", wndValidateWindowSize(pWindowRef->WindowWidth, pWindowRef->WindowHeight) ? "SUCCESS" : "FAIL");
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
            pWindowRef->Mode == WindowModeBorderless ? "Borderless" : (pWindowRef->Mode == WindowModeFullscreen ? "Fullscreen" : "Windowed"));
#endif
}

extern void wndSetWindowed(void* UserData);

extern void wndSetWindowed(void* UserData);
extern void wndSetFullscreen(void* UserData);
extern void wndSetBorderless(void* UserData);
extern void wndMaximizeWindow(void* UserData);
extern void wndMinimizeWindow(void* UserData);
extern void wndHideWindow();
extern void wndShowWindow();
extern void wndUpdateResolution(void* UserData);
extern void wndMoveWindow(void* UserData);
extern void wndSetRecommendedWindowSize(void* UserData);
extern void wndHideCursor();
extern void wndShowCursor();
extern void wndUpdateCaptureCursor(void* UserData);
extern void platformInitWindowSystem(WindowDesc* Data);
extern void platformExitWindowSystem();
extern void platformSetupWindowSystemUI(IApp* pApp)
{
#ifdef ENABLE_FORGE_UI
    float dpiScale;
    {
        float dpiScaleArray[2];
        GetMonitorDpiScale(pApp->settings.monitorIndex, dpiScaleArray);
        dpiScale = dpiScaleArray[0];
    }

    Vector2 UIPosition = { pApp->settings.width * 0.775f, pApp->settings.height * 0.01f };
    Vector2 UIPanelSize = Vector2(400.f, 750.f) / dpiScale;

    UIComponentDesc uiDesc = {};
    uiDesc.startPosition = UIPosition;
    uiDesc.startSize = UIPanelSize;
    uiDesc.fontID = 0;
    uiDesc.fontSize = 16.0f;
    uiCreateComponent("Window and Resolution Controls", &uiDesc, &pWindowControlsComponent);
    uiSetComponentFlags(pWindowControlsComponent, GUI_COMPONENT_FLAGS_START_COLLAPSED);

#if defined(_WINDOWS)
    bassignliteral(&gPlatformName, "Windows");
#elif defined(__APPLE__) && !defined(TARGET_IOS)
    bassignliteral(&gPlatformName, "MacOS");
#elif defined(__linux__) && !defined(__ANDROID__)
    bassignliteral(&gPlatformName, "Linux");
#else
    bassignliteral(&gPlatformName, "Unsupported");
#endif

    TextboxWidget Textbox;
    Textbox.pText = &gPlatformName;
    (uiCreateComponentWidget(pWindowControlsComponent, "Platform Name", &Textbox, WIDGET_TYPE_TEXTBOX));

#if defined(_WINDOWS) || defined(__APPLE__) && !defined(TARGET_IOS) || (defined(__linux__) && !defined(__ANDROID__))
    RadioButtonWidget rbWindowed;
    rbWindowed.Data = &pWindowRef->Mode;
    rbWindowed.radioId = WindowModeWindowed;
    UIWidget* pWindowed = uiCreateComponentWidget(pWindowControlsComponent, "Windowed", &rbWindowed, WIDGET_TYPE_RADIO_BUTTON);
    uiSetWidgetOnEditedCallback(pWindowed, nullptr, wndSetWindowed);
    uiSetWidgetDeferred(pWindowed, true);
    (pWindowed);

    RadioButtonWidget rbFullscreen;
    rbFullscreen.Data = &pWindowRef->Mode;
    rbFullscreen.radioId = WindowModeFullscreen;
    UIWidget* pFullscreen = uiCreateComponentWidget(pWindowControlsComponent, "Fullscreen", &rbFullscreen, WIDGET_TYPE_RADIO_BUTTON);
    uiSetWidgetOnEditedCallback(pFullscreen, nullptr, wndSetFullscreen);
    uiSetWidgetDeferred(pFullscreen, true);
    (pFullscreen);

    RadioButtonWidget rbBorderless;
    rbBorderless.Data = &pWindowRef->Mode;
    rbBorderless.radioId = WindowModeBorderless;
    UIWidget* pBorderless = uiCreateComponentWidget(pWindowControlsComponent, "Borderless", &rbBorderless, WIDGET_TYPE_RADIO_BUTTON);
    uiSetWidgetOnEditedCallback(pBorderless, nullptr, wndSetBorderless);
    uiSetWidgetDeferred(pBorderless, true);
    (pBorderless);

    ButtonWidget bMaximize;
    UIWidget*    pMaximize = uiCreateComponentWidget(pWindowControlsComponent, "Maximize", &bMaximize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pMaximize, nullptr, wndMaximizeWindow);
    uiSetWidgetDeferred(pMaximize, true);
    (pMaximize);

    ButtonWidget bMinimize;
    UIWidget*    pMinimize = uiCreateComponentWidget(pWindowControlsComponent, "Minimize", &bMinimize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pMinimize, nullptr, wndMinimizeWindow);
    uiSetWidgetDeferred(pMinimize, true);
    (pMinimize);

    CheckboxWidget rbCentered;
    rbCentered.Data = &(pWindowRef->Centered);
    (uiCreateComponentWidget(pWindowControlsComponent, "Toggle Window Centered", &rbCentered, WIDGET_TYPE_CHECKBOX));

    RectDesc recRes;
    GetRecommendedResolution(&recRes);

    uint32_t recWidth = recRes.right - recRes.left;
    uint32_t recHeight = recRes.bottom - recRes.top;

    SliderIntWidget setRectSliderX;
    setRectSliderX.Data = &pWindowRef->WindowX;
    setRectSliderX.min = 0;
    setRectSliderX.max = recWidth;
    (uiCreateComponentWidget(pWindowControlsComponent, "Window X Offset", &setRectSliderX, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderY;
    setRectSliderY.Data = &pWindowRef->WindowY;
    setRectSliderY.min = 0;
    setRectSliderY.max = recHeight;
    (uiCreateComponentWidget(pWindowControlsComponent, "Window Y Offset", &setRectSliderY, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderW;
    setRectSliderW.Data = &pWindowRef->WindowWidth;
    setRectSliderW.min = 144;
    setRectSliderW.max = GetRectWidth(&pWindowRef->FullscreenRect);
    (uiCreateComponentWidget(pWindowControlsComponent, "Window Width", &setRectSliderW, WIDGET_TYPE_SLIDER_INT));

    SliderIntWidget setRectSliderH;
    setRectSliderH.Data = &pWindowRef->WindowHeight;
    setRectSliderH.min = 144;
    setRectSliderH.max = GetRectHeight(&pWindowRef->FullscreenRect);
    (uiCreateComponentWidget(pWindowControlsComponent, "Window Height", &setRectSliderH, WIDGET_TYPE_SLIDER_INT));

    ButtonWidget bSetRect;
    UIWidget*    pSetRect = uiCreateComponentWidget(pWindowControlsComponent, "Set window rectangle", &bSetRect, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pSetRect, nullptr, wndMoveWindow);
    uiSetWidgetDeferred(pSetRect, true);
    (pSetRect);

    ButtonWidget bRecWndSize;
    UIWidget*    pRecWndSize =
        uiCreateComponentWidget(pWindowControlsComponent, "Set recommended window rectangle", &bRecWndSize, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pRecWndSize, nullptr, wndSetRecommendedWindowSize);
    uiSetWidgetDeferred(pRecWndSize, true);
    (pRecWndSize);

#if WINDOW_DETAILS
    uint      windowDetailsWidgetCount = 0;
    UIWidget  windowDetailsWidgets[32];
    UIWidget* windowDetailsWidgetGroup[32] = { 0 };

    TextboxWidget WindowedRectWidget = {};
    WindowedRectWidget.pText = &pWindowRef->pWindowedRectLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "WindowedRect");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &WindowedRectWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget FullscreenRectWidget = {};
    FullscreenRectWidget.pText = &pWindowRef->pFullscreenRectLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "FullscreenRect");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &FullscreenRectWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget ClientRectWidget = {};
    ClientRectWidget.pText = &pWindowRef->pClientRectLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "ClientRect");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &ClientRectWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget WndWidget = {};
    WndWidget.pText = &pWindowRef->pWndLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "Wnd");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &WndWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget FullscreenWidget = {};
    FullscreenWidget.pText = &pWindowRef->pFullscreenLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "Fullscreen");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &FullscreenWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget CursorCapturedWidget = {};
    CursorCapturedWidget.pText = &pWindowRef->pCursorCapturedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "CursorCaptured");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &CursorCapturedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget IconifiedWidget = {};
    IconifiedWidget.pText = &pWindowRef->pIconifiedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "Iconified");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &IconifiedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget MaximizedWidget = {};
    MaximizedWidget.pText = &pWindowRef->pMaximizedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "Maximized");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &MaximizedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget MinimizedWidget = {};
    MinimizedWidget.pText = &pWindowRef->pMinimizedLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "Minimized");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &MinimizedWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget NoResizeFrameWidget = {};
    NoResizeFrameWidget.pText = &pWindowRef->pNoResizeFrameLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "NoResizeFrame");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &NoResizeFrameWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget BorderlessWindowWidget = {};
    BorderlessWindowWidget.pText = &pWindowRef->pBorderlessWindowLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "BorderlessWindow");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &BorderlessWindowWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget OverrideDefaultPositionWidget = {};
    OverrideDefaultPositionWidget.pText = &pWindowRef->pOverrideDefaultPositionLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "OverrideDefaultPosition");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &OverrideDefaultPositionWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget CenteredWidget = {};
    CenteredWidget.pText = &pWindowRef->pCenteredLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "Centered");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &CenteredWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget ForceLowDPIWidget = {};
    ForceLowDPIWidget.pText = &pWindowRef->pForceLowDPILabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "ForceLowDPI");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &ForceLowDPIWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    TextboxWidget WindowModeWidget = {};
    WindowModeWidget.pText = &pWindowRef->pWindowModeLabel;
    windowDetailsWidgets[windowDetailsWidgetCount].type = WIDGET_TYPE_TEXTBOX;
    strcpy(windowDetailsWidgets[windowDetailsWidgetCount].label, "WindowMode");
    windowDetailsWidgets[windowDetailsWidgetCount].pWidget = &WindowModeWidget;
    windowDetailsWidgetGroup[windowDetailsWidgetCount] = &windowDetailsWidgets[windowDetailsWidgetCount];
    ++windowDetailsWidgetCount;

    CollapsingHeaderWidget windowDetailsHeader;
    windowDetailsHeader.widgetsCount = windowDetailsWidgetCount;
    windowDetailsHeader.pGroupedWidgets = windowDetailsWidgetGroup;
    uiCreateComponentWidget(pWindowControlsComponent, "Window Details", &windowDetailsHeader, WIDGET_TYPE_COLLAPSING_HEADER);
#endif

    uint32_t numMonitors = GetMonitorCount();

    char label[64];
    snprintf(label, 64, "Number of displays: ");

    char monitors[10];
    snprintf(monitors, 10, "%u", numMonitors);
    strcat(label, monitors);

    LabelWidget labelWidget;
    (uiCreateComponentWidget(pWindowControlsComponent, label, &labelWidget, WIDGET_TYPE_LABEL));

    for (uint32_t i = 0; i < numMonitors; ++i)
    {
        MonitorDesc* monitor = GetMonitor(i);

        char PublicDisplayName[128];
#if defined(_WINDOWS) || defined(XBOX) // Win platform uses wide chars
        if (128 == wcstombs(PublicDisplayName, monitor->PublicDisplayName, sizeof(PublicDisplayName)))
            PublicDisplayName[127] = '\0';
#elif !defined(TARGET_IOS)
        strcpy(PublicDisplayName, monitor->PublicDisplayName);
#endif
        char monitorLabel[128];
        snprintf(monitorLabel, 128, "%s", PublicDisplayName);
        strcat(monitorLabel, " (");

        char buffer[10];
        snprintf(buffer, 10, "%u", monitor->PhysicalSize[0]);
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, "x");

        snprintf(buffer, 10, "%u", monitor->PhysicalSize[1]);
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, " mm; ");

        snprintf(buffer, 10, "%u", monitor->Dpi[0]);
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, " Dpi; ");

        snprintf(buffer, 10, "%u", (unsigned)arrlen(monitor->Resolutions));
        strcat(monitorLabel, buffer);
        strcat(monitorLabel, " Resolutions)");

        UIWidget**         monitorWidgets = (UIWidget**)MemAllocate(arrlen(monitor->Resolutions) * sizeof(UIWidget*));
        UIWidget*          monitorWidgetsBases = (UIWidget*)MemAllocate(arrlen(monitor->Resolutions) * sizeof(UIWidget));
        RadioButtonWidget* radioWidgets = (RadioButtonWidget*)MemAllocate(arrlen(monitor->Resolutions) * sizeof(RadioButtonWidget));

        CollapsingHeaderWidget monitorHeader;
        monitorHeader.pGroupedWidgets = monitorWidgets;
        monitorHeader.widgetsCount = (uint32_t)arrlen(monitor->Resolutions);

        for (ptrdiff_t j = 0; j < arrlen(monitor->Resolutions); ++j)
        {
            Resolution res = monitor->Resolutions[j];

            radioWidgets[j].Data = &pWindowRef->CurrentResolution[i];
            radioWidgets[j].radioId = (int32_t)j;

            monitorWidgets[j] = &monitorWidgetsBases[j];
            monitorWidgetsBases[j] = UIWidget{};
            monitorWidgetsBases[j].type = WIDGET_TYPE_RADIO_BUTTON;
            monitorWidgetsBases[j].pWidget = &radioWidgets[j];

            snprintf(monitorWidgetsBases[j].label, MAX_LABEL_STR_LENGTH, "%u", res.width);
            strcat(monitorWidgetsBases[j].label, "x");

            char height[10];
            snprintf(height, 10, "%u", res.height);
            strcat(monitorWidgetsBases[j].label, height);

            if (monitor->DefaultResolution.width == res.width && monitor->DefaultResolution.height == res.height)
            {
                strcat(monitorWidgetsBases[j].label, " (native)");
                pWindowRef->CurrentResolution[i] = (int32_t)j;
                pWindowRef->LastResolution[i] = (int32_t)j;
            }

            uiSetWidgetOnEditedCallback(monitorWidgets[j], nullptr, wndUpdateResolution);
            uiSetWidgetDeferred(monitorWidgets[j], false);
        }

        uiCreateComponentWidget(pWindowControlsComponent, monitorLabel, &monitorHeader, WIDGET_TYPE_COLLAPSING_HEADER);
        MemDeallocate(radioWidgets);
        MemDeallocate(monitorWidgetsBases);
        MemDeallocate(monitorWidgets);
    }

    enum
    {
        CONTROLS_INSIDE_WINDOW_LABEL_WIDGET,
        CONTROLS_INSIDE_WINDOW_NO_WIDGET,
        CONTROLS_INSIDE_WINDOW_YES_WIDGET,
        CONTROLS_CLIP_CURSOR_WIDGET,

        CONTROLS_WIDGET_COUNT
    };

    UIWidget  inputControlsWidgetBases[CONTROLS_WIDGET_COUNT];
    UIWidget* inputControlsWidgets[CONTROLS_WIDGET_COUNT];
    for (int i = 0; i < CONTROLS_WIDGET_COUNT; ++i)
    {
        inputControlsWidgetBases[i] = UIWidget{};
        inputControlsWidgets[i] = &inputControlsWidgetBases[i];
    }
    CollapsingHeaderWidget inputControlsWidget;
    inputControlsWidget.pGroupedWidgets = inputControlsWidgets;
    inputControlsWidget.widgetsCount = CONTROLS_WIDGET_COUNT;

    LabelWidget lCursorInWindow;
    strcpy(inputControlsWidgets[CONTROLS_INSIDE_WINDOW_LABEL_WIDGET]->label, "Cursor inside window?");
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_LABEL_WIDGET]->pWidget = &lCursorInWindow;
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_LABEL_WIDGET]->type = WIDGET_TYPE_LABEL;

    RadioButtonWidget rCursorInsideRectFalse;
    rCursorInsideRectFalse.Data = &pWindowRef->CursorInsideWindow;
    rCursorInsideRectFalse.radioId = 0;
    strcpy(inputControlsWidgets[CONTROLS_INSIDE_WINDOW_NO_WIDGET]->label, "No");
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_NO_WIDGET]->pWidget = &rCursorInsideRectFalse;
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_NO_WIDGET]->type = WIDGET_TYPE_RADIO_BUTTON;

    RadioButtonWidget rCursorInsideRectTrue;
    rCursorInsideRectTrue.Data = &pWindowRef->CursorInsideWindow;
    rCursorInsideRectTrue.radioId = 1;
    strcpy(inputControlsWidgets[CONTROLS_INSIDE_WINDOW_YES_WIDGET]->label, "Yes");
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_YES_WIDGET]->pWidget = &rCursorInsideRectTrue;
    inputControlsWidgets[CONTROLS_INSIDE_WINDOW_YES_WIDGET]->type = WIDGET_TYPE_RADIO_BUTTON;

    CheckboxWidget bClipCursor;
    bClipCursor.Data = &pWindowRef->CursorCaptureRequested;

    strcpy(inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET]->label, "Capture cursor");
    inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET]->pWidget = &bClipCursor;
    inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET]->type = WIDGET_TYPE_CHECKBOX;
    uiSetWidgetOnEditedCallback(inputControlsWidgets[CONTROLS_CLIP_CURSOR_WIDGET], nullptr, wndUpdateCaptureCursor);

    (uiCreateComponentWidget(pWindowControlsComponent, "Cursor", &inputControlsWidget, WIDGET_TYPE_COLLAPSING_HEADER));
#endif

#if defined(AUTOMATED_TESTING)
    LabelWidget lScreenshotName;
    (uiCreateComponentWidget(pWindowControlsComponent, "Screenshot Name", &lScreenshotName, WIDGET_TYPE_LABEL));

    snprintf(gAppName, sizeof(gAppName), "%s", pApp->GetName());

    TextboxWidget bTakeScreenshotName;
    bTakeScreenshotName.pText = &gScriptName;
    UIWidget* pTakeScreenshotName =
        uiCreateComponentWidget(pWindowControlsComponent, "Screenshot Name", &bTakeScreenshotName, WIDGET_TYPE_TEXTBOX);
    (pTakeScreenshotName);

    ButtonWidget bTakeScreenshot;
    UIWidget* pTakeScreenshot = uiCreateComponentWidget(pWindowControlsComponent, "Take Screenshot", &bTakeScreenshot, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pTakeScreenshot, nullptr, wndTakeScreenshot);
    uiSetWidgetDeferred(pTakeScreenshot, true);
    (pTakeScreenshot);
#endif

#endif
}

void platformToggleWindowSystemUI(bool active)
{
#ifdef ENABLE_FORGE_UI
    uiSetComponentActive(pWindowControlsComponent, active);
#endif
}
