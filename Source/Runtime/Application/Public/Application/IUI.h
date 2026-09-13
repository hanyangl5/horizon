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

#ifndef IUI_H
#define IUI_H

#include "Core/IConfig.h"

// SCRIPTED TESTING :
// For now, if a script file with the name "Test.lua", exist in the script directory, will run once an execution.
// Lua function name resolution:
// - UI Widget "label"s will be included in the name
//		- For Widget events: label name + "Event Name". e.g., Lua Function name for label - "Press", event - OnEdited : "PressOnEdited"
//		- For Widget modifier ints/floats: "Set" and "Get" function set will be added as a prefix to label name.
//											e.g., "X" variable will have "SetX" and "GetX" pair of functions
// To add global Lua functions, independent of Unit Tests, add definition in UIApp::Init (Check LOGINFO there for example).

#include <ThirdParty/stb/stb_ds.h>
#include <ThirdParty/bstrlib/bstrlib.h>

#include "Core/IMath.h"

struct UserInterfaceDrawData;

typedef struct Renderer      Renderer;
typedef struct Cmd           Cmd;
typedef struct RenderTarget  RenderTarget;
typedef struct PipelineCache PipelineCache;

#define MAX_LABEL_STR_LENGTH  128
#define MAX_FORMAT_STR_LENGTH 30
#define MAX_TITLE_STR_LENGTH  128

/****************************************************************************/
// MARK: - UI Widget Data Structures
/****************************************************************************/
typedef void (*WidgetCallback)(void* pUserData);
typedef void (*WindowCallback)(void* pUserData);

enum WidgetType
{
    WIDGET_TYPE_COLLAPSING_HEADER,
    WIDGET_TYPE_DEBUG_TEXTURES,
    WIDGET_TYPE_LABEL,
    WIDGET_TYPE_COLOR_LABEL,
    WIDGET_TYPE_HORIZONTAL_SPACE,
    WIDGET_TYPE_SEPARATOR,
    WIDGET_TYPE_VERTICAL_SEPARATOR,
    WIDGET_TYPE_BUTTON,
    WIDGET_TYPE_SLIDER_FLOAT,
    WIDGET_TYPE_SLIDER_FLOAT2,
    WIDGET_TYPE_SLIDER_FLOAT3,
    WIDGET_TYPE_SLIDER_FLOAT4,
    WIDGET_TYPE_SLIDER_INT,
    WIDGET_TYPE_SLIDER_UINT,
    WIDGET_TYPE_RADIO_BUTTON,
    WIDGET_TYPE_CHECKBOX,
    WIDGET_TYPE_ONE_LINE_CHECKBOX,
    WIDGET_TYPE_CURSOR_LOCATION,
    WIDGET_TYPE_DROPDOWN,
    WIDGET_TYPE_COLUMN,
    WIDGET_TYPE_PROGRESS_BAR,
    WIDGET_TYPE_COLOR_SLIDER,
    WIDGET_TYPE_HISTOGRAM,
    WIDGET_TYPE_PLOT_LINES,
    WIDGET_TYPE_COLOR_PICKER,
    WIDGET_TYPE_COLOR3_PICKER,
    WIDGET_TYPE_TEXTBOX,
    WIDGET_TYPE_DYNAMIC_TEXT,
    WIDGET_TYPE_FILLED_RECT,
    WIDGET_TYPE_DRAW_TEXT,
    WIDGET_TYPE_DRAW_TOOLTIP,
    WIDGET_TYPE_DRAW_LINE,
    WIDGET_TYPE_DRAW_CURVE,
    WIDGET_TYPE_CUSTOM
};

typedef struct UIWidget
{
    WidgetType type = {};     // Type of the underlying widget
    void*      pWidget = NULL; // Underlying widget

    void*          pOnHoverUserData = NULL;
    WidgetCallback pOnHover = NULL; // Widget is hovered, usable, and not blocked by anything.
    void*          pOnActiveUserData = NULL;
    WidgetCallback pOnActive = NULL; // Widget is currently active (ex. button being held)
    void*          pOnFocusUserData = NULL;
    WidgetCallback pOnFocus = NULL; // Widget is currently focused (for keyboard/gamepad nav)
    void*          pOnEditedUserData = NULL;
    WidgetCallback pOnEdited = NULL; // Widget just changed its underlying value or was pressed.
    void*          pOnDeactivatedUserData = NULL;
    WidgetCallback pOnDeactivated = NULL; // Widget was just made inactive from an active state.  This is useful for undo/redo patterns.
    void*          pOnDeactivatedAfterEditUserData = NULL;
    WidgetCallback pOnDeactivatedAfterEdit = NULL; // Widget was just made inactive from an active state and changed its underlying value.
                                                   // This is useful for undo/redo patterns.

    char label[MAX_LABEL_STR_LENGTH]{};

    // Set this to process deferred callbacks that may cause global program state changes.
    bool deferred = false;

    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool edited = false;
    bool deactivated = false;
    bool deactivatedAfterEdit = false;
    bool sameLine = false;

    // Stores the screen space position of the widget
    float2 displayPosition;
} UIWidget;

typedef struct CollapsingHeaderWidget
{
    // array of UIWidget*
    UIWidget** pGroupedWidgets = NULL;
    uint32_t   widgetsCount = 0;
    bool       collapsed = false;
    bool       previousCollapsed = false;
    bool       defaultOpen = false;
    bool       headerIsVisible = true;
} CollapsingHeaderWidget;

typedef struct ColumnWidget
{
    // array of UIWidget*
    UIWidget** pPerColumnWidgets = NULL;
    uint32_t   widgetsCount = 0;
} ColumnWidget;

struct Texture;

typedef struct DebugTexturesWidget
{
    // C Array of const Texture*
    const struct Texture* const* pTextures = NULL;
    uint32_t                     texturesCount = 0;
    float2                       textureDisplaySize = float2(512.f, 512.f);

} DebugTexturesWidget;

typedef struct LabelWidget
{
} LabelWidget;

typedef struct ColorLabelWidget
{
    float4 color = float4(0.f, 0.f, 0.f, 0.f);
} ColorLabelWidget;

typedef struct HorizontalSpaceWidget
{
} HorizontalSpaceWidget;

typedef struct SeparatorWidget
{
} SeparatorWidget;

typedef struct VerticalSeparatorWidget
{
    uint32_t lineCount = 0;
} VerticalSeparatorWidget;

typedef struct ButtonWidget
{
} ButtonWidget;

typedef struct SliderFloatWidget
{
    char   format[MAX_FORMAT_STR_LENGTH] = { "%.3f" };
    float* pData = NULL;
    float  min = 0.f;
    float  max = 0.f;
    float  step = 0.01f;
} SliderFloatWidget;

typedef struct SliderFloat2Widget
{
    char    format[MAX_FORMAT_STR_LENGTH] = { "%.3f" };
    float2* pData = NULL;
    float2  min = float2(0.f, 0.f);
    float2  max = float2(0.f, 0.f);
    float2  step = float2(0.01f, 0.01f);
} SliderFloat2Widget;

typedef struct SliderFloat3Widget
{
    char    format[MAX_FORMAT_STR_LENGTH] = { "%.3f" };
    float3* pData = NULL;
    float3  min = float3(0.f, 0.f, 0.f);
    float3  max = float3(0.f, 0.f, 0.f);
    float3  step = float3(0.01f, 0.01f, 0.01f);
} SliderFloat3Widget;

typedef struct SliderFloat4Widget
{
    char    format[MAX_FORMAT_STR_LENGTH] = { "%.3f" };
    float4* pData = NULL;
    float4  min = float4(0.f, 0.f, 0.f, 0.f);
    float4  max = float4(0.f, 0.f, 0.f, 0.f);
    float4  step = float4(0.01f, 0.01f, 0.01f, 0.01f);
} SliderFloat4Widget;

typedef struct SliderIntWidget
{
    char     format[MAX_FORMAT_STR_LENGTH] = { "%d" };
    int32_t* pData = NULL;
    int32_t  min = 0;
    int32_t  max = 0;
    int32_t  step = 1;
} SliderIntWidget;

typedef struct SliderUintWidget
{
    char      format[MAX_FORMAT_STR_LENGTH] = { "%u" };
    uint32_t* pData = NULL;
    uint32_t  min = 0;
    uint32_t  max = 0;
    uint32_t  step = 1;
} SliderUintWidget;

typedef struct RadioButtonWidget
{
    int32_t* pData = NULL;
    int32_t  radioId = 0;
} RadioButtonWidget;

typedef struct CheckboxWidget
{
    bool* pData = NULL;
} CheckboxWidget;

typedef struct OneLineCheckboxWidget
{
    bool*  pData = NULL;
    float4 color = float4(0.f, 0.f, 0.f, 0.f);
} OneLineCheckboxWidget;

typedef struct CursorLocationWidget
{
    float2 location = float2(0.f, 0.f);
} CursorLocationWidget;

typedef struct DropdownWidget
{
    uint32_t*          pData = NULL;
    // pNames is a C array of size count
    const char* const* pNames = NULL;
    uint32_t           count = 0;
} DropdownWidget;

typedef struct ProgressBarWidget
{
    size_t* pData = NULL;
    size_t  maxProgress = 0;
} ProgressBarWidget;

typedef struct ColorSliderWidget
{
    float4* pData = NULL;
} ColorSliderWidget;

typedef struct HistogramWidget
{
    float*      pValues = NULL;
    uint32_t    count = 0;
    float*      minScale = NULL;
    float*      maxScale = NULL;
    float2      histogramSize = float2(0.f, 0.f);
    const char* histogramTitle = NULL;
} HistogramWidget;

typedef struct PlotLinesWidget
{
    float*      values = NULL;
    uint32_t    numValues = 0;
    float*      scaleMin = NULL;
    float*      scaleMax = NULL;
    float2*     plotScale = NULL;
    const char* title = NULL;
} PlotLinesWidget;

typedef struct ColorPickerWidget
{
    float4* pData = NULL;
} ColorPickerWidget;

typedef struct Color3PickerWidget
{
    float3* pData = NULL;
} Color3PickerWidget;

typedef enum UITextFlags
{
    UI_TEXT_ENABLE_RESIZE = 0x1,
    UI_TEXT_AUTOSELECT_ALL = 0x2
} TextFlags;

typedef void (*TextboxCallback)(bool* keysDown);

typedef struct TextboxWidget
{
    bstring*        pText = NULL;
    unsigned char   flags = UI_TEXT_AUTOSELECT_ALL;
    TextboxCallback pCallback = NULL;
} TextboxWidget;

typedef struct DynamicTextWidget
{
    bstring*      pText = NULL;
    float4*       pColor = NULL;
    unsigned char flags = 0;
} DynamicTextWidget;

typedef struct FilledRectWidget
{
    float2 pos = float2(0.f, 0.f);
    float2 scale = float2(0.f, 0.f);
    float4 color = float4(0.f, 0.f, 0.f, 0.f);
} FilledRectWidget;

typedef struct DrawTextWidget
{
    float2 pos = float2(0.f, 0.f);
    float4 color = float4(0.f, 0.f, 0.f, 0.f);
} DrawTextWidget;

typedef struct DrawTooltipWidget
{
    bool* showTooltip = NULL;
    char* text = NULL;
} DrawTooltipWidget;

typedef struct DrawLineWidget
{
    float2 pos1 = float2(0.f, 0.f);
    float2 pos2 = float2(0.f, 0.f);
    float4 color = float4(0.f, 0.f, 0.f, 0.f);
    bool   addItem = false;
} DrawLineWidget;

typedef struct DrawCurveWidget
{
    float2*  pos = NULL;
    uint32_t numPoints = 0;
    float    thickness = 0.f;
    float4   color = float4(0.f, 0.f, 0.f, 0.f);
} DrawCurveWidget;

typedef struct CustomWidget
{
    void*          pUserData;
    WidgetCallback pCallback;
    WidgetCallback pDestroyCallback;
} CustomWidget;

/****************************************************************************/
// MARK: - UI Component Data Structures
/****************************************************************************/

enum GuiComponentFlags
{
    GUI_COMPONENT_FLAGS_NONE = 0,
    GUI_COMPONENT_FLAGS_NO_TITLE_BAR = 1 << 0,          // Disable title-bar
    GUI_COMPONENT_FLAGS_NO_RESIZE = 1 << 1,             // Disable user resizing
    GUI_COMPONENT_FLAGS_NO_MOVE = 1 << 2,               // Disable user moving the window
    GUI_COMPONENT_FLAGS_NO_SCROLLBAR = 1 << 3,          // Disable scrollbars (window can still scroll with mouse or programatically)
    GUI_COMPONENT_FLAGS_NO_COLLAPSE = 1 << 4,           // Disable user collapsing window by double-clicking on it
    GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE = 1 << 5,    // Resize every window to its content every frame
    GUI_COMPONENT_FLAGS_NO_INPUTS = 1 << 6,             // Disable catching mouse or keyboard inputs, hovering test with pass through.
    GUI_COMPONENT_FLAGS_MEMU_BAR = 1 << 7,              // Has a menu-bar
    GUI_COMPONENT_FLAGS_HORIZONTAL_SCROLLBAR = 1 << 8,  // Allow horizontal scrollbar to appear (off by default).
    GUI_COMPONENT_FLAGS_NO_FOCUS_ON_APPEARING = 1 << 9, // Disable taking focus when transitioning from hidden to visible state
    GUI_COMPONENT_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS =
        1 << 10, // Disable bringing window to front when taking focus (e.g. clicking on it or programatically giving it focus)
    GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR = 1 << 11,   // Always show vertical scrollbar (even if ContentSize.y < Size.y)
    GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR = 1 << 12, // Always show horizontal scrollbar (even if ContentSize.x < Size.x)
    GUI_COMPONENT_FLAGS_ALWAYS_USE_WINDOW_PADDING = 1 << 13,   // Ensure child windows without border uses style.WindowPadding (ignored by
                                                               // default for non-bordered child windows, because more convenient)
    GUI_COMPONENT_FLAGS_NO_NAV_INPUT = 1 << 14,                // No gamepad/keyboard navigation within the window
    GUI_COMPONENT_FLAGS_NO_NAV_FOCUS =
        1 << 15, // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
    GUI_COMPONENT_FLAGS_START_COLLAPSED = 1 << 16,
    GUI_COMPONENT_FLAGS_NO_DOCKING = 1 << 17
};

typedef struct UIComponentDesc
{
    vec2 startPosition = vec2{ 0.0f, 150.0f };
    vec2 startSize = vec2{ 600.0f, 550.0f };

    uint32_t fontID = 0;
    float    fontSize = 16.0f;
} UIComponentDesc;

typedef struct UIComponent
{
    //(UIWidget*)[dyn_size]
    UIWidget** widgets = NULL;
    //(bool)[dyn_size]
    bool*      widgetsClone = NULL;
    void*      pUserData = NULL;

    // Contextual menus when right clicking the title bar
    char const* const*    contextualMenuLabels = NULL;
    WidgetCallback const* contextualMenuCallbacks = NULL;
    size_t                contextualMenuCount = 0;
    float4                initialWindowRect = float4(0.f, 0.f, 0.f, 0.f);
    float4                currentWindowRect = float4(0.f, 0.f, 0.f, 0.f);
    char                  title[MAX_TITLE_STR_LENGTH] = { 0 };
    uintptr_t             pFont = 0;
    uint32_t              fontTextureIndex = 0;
    float                 alpha = 0.f;

    // defaults to GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE
    // on mobile, GUI_COMPONENT_FLAGS_START_COLLAPSED is also set
    int32_t flags = 0;

    bool active = false;

    // UI Component settings that can be modified at runtime by the client.
    bool hasCloseButton = false;

    // Custom callbacks for raw driver API calls
    WindowCallback pPreProcessCallback = NULL;
    WindowCallback pPostProcessCallback = NULL;
} UIComponent;

/****************************************************************************/
// MARK: - Dynamic UI Widget Data Structures
/****************************************************************************/

typedef struct DynamicUIWidgets
{
    // stb_ds array of UIWidget*
    UIWidget** dynamicProperties = NULL;
} DynamicUIWidgets;

/****************************************************************************/
// MARK: - Forge User Interface Data Structures
/****************************************************************************/

typedef struct UserInterfaceDesc
{
    Renderer*      pRenderer = NULL;
    PipelineCache* pCache = NULL;
    char const*    settingsFilename = nullptr;

    uint32_t maxDynamicUIUpdatesPerBatch = 20u;
    uint32_t maxUIFonts = 10u;

    uint32_t frameCount = 2u;
    bool     enableDocking = false;
    bool     enableRemoteUI = false;
} UserInterfaceDesc;

typedef struct UserInterfaceLoadDesc
{
    PipelineCache* pCache;
    uint32_t       loadType;    // enum ReloadType
    uint32_t       colorFormat; // enum TinyImageFormat
    uint32_t       width;
    uint32_t       height;
    uint32_t       displayWidth;
    uint32_t       displayHeight;
} UserInterfaceLoadDesc;

typedef struct UserInterfaceDrawCommand
{
    float4   clipRect;
    uint64_t textureId;
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t elemCount;
} UserInterfaceDrawElement;

typedef struct UserInterfaceDrawData
{
    uint32_t                  vertexCount;
    uint32_t                  indexCount;
    uint32_t                  vertexSize;
    uint32_t                  indexSize;
    float2                    displayPos;
    float2                    displaySize;
    uint32_t                  numDrawCommands;
    unsigned char*            vertexBufferData;
    unsigned char*            indexBufferData;
    UserInterfaceDrawCommand* drawCommands;
} UserInterfaceDrawData;

/****************************************************************************/
// MARK: - Application Life Cycle
/****************************************************************************/

/// Initializes the Forge Rendering objects associated with the User Interface
/// The Forge's User Interface makes use of ImGUI
/// To be called at application initialization time by the App Layer
FORGE_API void initUserInterface(UserInterfaceDesc* pDesc);

/// Frees Forge Rendering objects and memory associated with the User Interface
/// To be called at application shutdown time by the App Layer
FORGE_API void exitUserInterface();

/// Creates graphics pipelines associated with the User Interface
/// To be called at application load time by the App Layer
FORGE_API void loadUserInterface(const UserInterfaceLoadDesc* pDesc);

/// Destroys graphics pipelines associated with the User Interface
/// To be called at application unload time by the App Layer
FORGE_API void unloadUserInterface(uint32_t unloadType);

/// Renders defined ImGUI components and widgets using The Forge's Renderer
/// This function also handles rendering the Forge Profiler's UI Window.
/// If pUIDrawData* is NULL, the current ImGUI state will be used, otherwise the passed in draw data will be used.
/// Due to the nature of ImGUI not being thread safe, this call must be made on the main thread if pUIDrawData* is kept NULL.
FORGE_API void cmdDrawUserInterface(Cmd* pCmd, UserInterfaceDrawData* pUIDrawData = NULL);

/****************************************************************************/
// MARK: - Collapsing Header Widget Public Functions
/****************************************************************************/

/// Set whether or not a given Collapsing Header widget is currently collapsed
inline void uiSetCollapsingHeaderWidgetCollapsed(CollapsingHeaderWidget* pWidget, bool collapsed)
{
#ifdef ENABLE_FORGE_UI
    pWidget->collapsed = collapsed;
    pWidget->previousCollapsed = !collapsed;
#endif
}

/****************************************************************************/
// MARK: - Dynamic Widget Public Functions
/****************************************************************************/

/// Create an independent set of widgets which can be dynamically added to a UI Component
FORGE_API UIWidget* uiCreateDynamicWidgets(DynamicUIWidgets* pDynamicUI, const char* pLabel, const void* pWidget, WidgetType type);

/// Free memory associated with a set of dynamic UI widgets
FORGE_API void uiDestroyDynamicWidgets(DynamicUIWidgets* pDynamicUI);

/// Add an existing set of dynamic widgets to an existing UI Component
FORGE_API void uiShowDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui);

/// Remove an existing set of dynamic widgets from an existing UI Component
FORGE_API void uiHideDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui);

/****************************************************************************/
// MARK: - UI Component Public Functions
/****************************************************************************/

/// Create a UI Component "window" to which Widgets can be added
/// User is NOT responsible for freeing this memory at application exit
FORGE_API void uiCreateComponent(const char* pTitle, const UIComponentDesc* pDesc, UIComponent** ppGuiComponent);

/// Free memory associated with a UI Component "window"
/// Only necessary for replacement purposes. UI Component memory will be freed internally on exit
FORGE_API void uiDestroyComponent(UIComponent* pGui);

/// Set whether or not a given UI Component is active and visible on the screen
FORGE_API void uiSetComponentActive(UIComponent* pGuiComponent, bool active);

/// Create a Widget to be assigned to a given UI Component
/// User is NOT responsible for freeing this memory at application exit
FORGE_API UIWidget* uiCreateComponentWidget(UIComponent* pGui, const char* pLabel, const void* pWidget, WidgetType type, //-V1071
                                            bool clone = true);

/// Destroy and free memory associated with a Widget
/// Only necessary for replacement purposes. UI Widget memory will be freed internally on exit
FORGE_API void uiDestroyComponentWidget(UIComponent* pGui, UIWidget* pWidget);

/// Destroy and free memory associated with all Widgets in a given UI Component
/// Only necessary for replacement purposes. UI Widget memory will be freed internally on exit
FORGE_API void uiDestroyAllComponentWidgets(UIComponent* pGui);

/****************************************************************************/
// MARK: - Safe UI Component and Widget Setter Functions
/****************************************************************************/

// NOTE: These functions exist to protect scope against null-pointer derefs
// on UI Component and Widget handles if this functionality still exists in
// app code while the UI Master Switch is disabled.

/// Assign "GuiComponentFlags" enum values to a given UI Component
FORGE_API void uiSetComponentFlags(UIComponent* pGui, int32_t flags);

/// Set whether or not a given Widget is intended to be "deferrred"
FORGE_API void uiSetWidgetDeferred(UIWidget* pWidget, bool deferred);

/// Assign Widget callback function (pointer to a function which takes and returns void)
/// Will be called when Widget is hovered, usable, and not blocked by anything
FORGE_API void uiSetWidgetOnHoverCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback);

/// Assign Widget callback function (pointer to a function which takes and returns void)
/// Will be called when Widget is currently active (ex. button being held)
FORGE_API void uiSetWidgetOnActiveCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback);

/// Assign Widget callback function (pointer to a function which takes and returns void)
/// Will be called when Widget is currently focused (for keyboard/gamepad nav)
FORGE_API void uiSetWidgetOnFocusCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback);

/// Assign Widget callback function (pointer to a function which takes and returns void)
/// Will be called when Widget just changed its underlying value or was pressed
FORGE_API void uiSetWidgetOnEditedCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback);

/// Assign Widget callback function (pointer to a function which takes and returns void)
/// Will be called when Widget is made inactive from an active state
FORGE_API void uiSetWidgetOnDeactivatedCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback);

/// Assign Widget callback function (pointer to a function which takes and returns void)
/// Will be called when Widget is made inactive from an active state and its underlying value has changed
FORGE_API void uiSetWidgetOnDeactivatedAfterEditCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback);

/// Set whether or not a given UI Widget should be on the sameline than the previous one
FORGE_API void uiSetSameLine(UIWidget* pGuiComponent, bool sameLine);

/****************************************************************************/
// MARK: - Other User Interface Functionality
/****************************************************************************/

/// Returns whether or not the UI is currently "focused" by the cursor
FORGE_API bool uiIsFocused();

/// Callback function to share any type of input data w/ ImGUI.
FORGE_API void uiOnInput(uint32_t actionId, bool buttonPress, const float2* pMousePos, const float2* pStick);

/// Callback function to share text entry data w/ ImGUI
FORGE_API void uiOnText(const wchar_t* pText);

/// Returns value associated with UI preparedness to accept text entry data
/// 0 -> Not pressed, 1 -> Digits Only keyboard, 2 -> Full Keyboard (Chars + Digits)
FORGE_API uint8_t uiWantTextInput();

/// Toggle UI rendering, input processing still works (used to hide UI components while taking screenshots)
FORGE_API void uiToggleRendering(bool enabled);

/// Returns whether or not UI rendering is currently enabled (see uiToggleRendering)
FORGE_API bool uiIsRenderingEnabled();

/// Creates a UserInterfaceDrawData* which can be passed to cmdDrawUserInterface(Cmd* pCmd, UserInterfaceDrawData* pUIDrawData) explicitly.
/// It's the caller's responsibility to call removeUIDrawData(...) when done.
FORGE_API UserInterfaceDrawData* addUIDrawData();

/// Populates a UserInterfaceDrawData* with current frame draw data so that it can be passed to cmdDrawUserInterface(Cmd* pCmd,
/// UserInterfaceDrawData* pUIDrawData) at a later time.
FORGE_API void uiPopulateDrawData(UserInterfaceDrawData* pUIDrawData);

/// Removes a UserInterfaceDrawData*.  It should not be reused after this point.
FORGE_API void removeUIDrawData(UserInterfaceDrawData* pUIDrawData);

#endif // IUI_H
