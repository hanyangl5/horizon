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
#include <ThirdParty/tinyimageformat/tinyimageformat_base.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>
#include <ThirdParty/imgui/imgui_internal.h>
#include <ThirdParty/imgui/imgui.h>
#include "Application/IFont.h"
#include "Platform/IInput.h"
#include "Application/IUI.h"
#include "Resources/IResourceLoader.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"

#include "Core/IMath.h"

#include "Core/IMemory.h"

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#define TOUCH_INPUT 1
#endif

#define FALLBACK_FONT_TEXTURE_INDEX 0

#define MAX_LABEL_LENGTH            128

#define LABELID(prop, buffer)       snprintf(buffer, MAX_LABEL_LENGTH, "##%llu", (unsigned long long)(prop.pData))
#define LABELID1(prop, buffer)      snprintf(buffer, MAX_LABEL_LENGTH, "##%llu", (unsigned long long)(prop))

#define MAX_LUA_STR_LEN             256
static const uint32_t MAX_FRAMES = 3;

#define FORGE_UI_USE_32BIT_INDEXES
#define FORGE_UI_MAX_VERTEXES (64 * 1024)
#define FORGE_UI_MAX_INDEXES  (128 * 1024)

struct GUIDriverUpdate
{
    UIComponent** pUIComponents = NULL;
    uint32_t      componentCount = 0;
    float         deltaTime = 0.f;
    float         width = 0.f;
    float         height = 0.f;
    bool          showDemoWindow = false;
};

struct UIAppImpl
{
    Renderer*     pRenderer = NULL;
    // stb_ds array of UIComponent*
    UIComponent** components = NULL;
};

typedef struct UserInterface
{
    float    width = 0.f;
    float    height = 0.f;
    float    displayWidth = 0.f;
    float    displayHeight = 0.f;
    uint32_t maxDynamicUIUpdatesPerBatch = 20u;
    uint32_t maxUIFonts = 10u;
    uint32_t frameCount = 2;

    // Following var is useful for seeing UI capabilities and tweaking style settings.
    // Will only take effect if at least one GUI Component is active.
    bool showDemoUiWindow = false;

    Renderer*     pRenderer = NULL;
    // stb_ds array of UIComponent*
    UIComponent** components = NULL;

    PipelineCache* pPipelineCache = NULL;
    ImGuiContext*  context = NULL;
    // (Texture*)[dyn_size]
    struct UIFontResource
    {
        Texture*  pFontTex = NULL;
        uint32_t  fontId = 0;
        float     fontSize = 0.f;
        uintptr_t pFont = 0;
    }* pCachedFontsArr = NULL;
    uintptr_t pDefaultFallbackFont = 0;
    float     dpiScale[2] = { 0.0f };
    uint32_t  frameIdx = 0;

    struct TextureNode
    {
        uint64_t key = ~0ull;
        Texture* value = NULL;
    }* pTextureHashmap = NULL;
    uint32_t       dynamicTexturesCount = 0;
    Shader*        pShaderTextured[SAMPLE_COUNT_COUNT] = { NULL };
    RootSignature* pRootSignatureTextured = NULL;
    DescriptorSet* pDescriptorSetUniforms = NULL;
    DescriptorSet* pDescriptorSetTexture = NULL;
    Pipeline*      pPipelineTextured[SAMPLE_COUNT_COUNT] = { NULL };
    Buffer*        pVertexBuffer = NULL;
    Buffer*        pIndexBuffer = NULL;
    Buffer*        pUniformBuffer[MAX_FRAMES] = { NULL };
    /// Default states
    Sampler*       pDefaultSampler = NULL;
    VertexLayout   vertexLayoutTextured = {};
    float          navInputs[ImGuiNavInput_COUNT] = { 0.0f };
    const float2*  pMovePosition = NULL;
    uint32_t       lastUpdateCount = 0;
    float2         lastUpdateMin[64] = {};
    float2         lastUpdateMax[64] = {};
    bool           active = false;
    bool           postUpdateKeyDownStates[ImGuiKey::ImGuiKey_COUNT] = { false };

    // Since gestures events always come first, we want to dismiss any other inputs after that
    bool handledGestures = false;

    // Stops rendering UI elements (disables command recording)
    bool enableRendering = true;

    // Disabled by default for the single-client path.
    bool enableRemoteUI = false;
} UserInterface;

#if defined(ENABLE_FORGE_REMOTE_UI)
extern void initRemoteAppServer(uint16_t port);
extern void exitRemoteAppServer();
extern void remoteAppSendDrawData(UserInterfaceDrawData* pDrawData);
extern void remoteAppReceiveInputData();
extern bool remoteAppIsConnected();
extern bool remoteAppShouldSendFontTexture();
extern void remoteControlSendTexture(TinyImageFormat format, uint64_t textureId, uint32_t width, uint32_t height, uint32_t size,
                                     unsigned char* ptr);
#endif

#if defined(GFX_DRIVER_MEMORY_TRACKING) || defined(GFX_DEVICE_MEMORY_TRACKING)
extern uint32_t    GetTrackedObjectTypeCount();
extern const char* GetTrackedObjectName(uint32_t obj);
#endif

#if defined(GFX_DRIVER_MEMORY_TRACKING)
// Driver memory getters
extern uint64_t GetDriverAllocationsCount();
extern uint64_t GetDriverMemoryAmount();
extern uint64_t GetDriverAllocationsPerObject(uint32_t obj);
extern uint64_t GetDriverMemoryPerObject(uint32_t obj);
#endif

#if defined(GFX_DEVICE_MEMORY_TRACKING)
// Device memory getters
extern uint64_t GetDeviceAllocationsCount();
extern uint64_t GetDeviceMemoryAmount();
extern uint64_t GetDeviceAllocationsPerObject(uint32_t obj);
extern uint64_t GetDeviceMemoryPerObject(uint32_t obj);
#endif

#ifdef ENABLE_FORGE_UI
static UserInterface* pUserInterface = NULL;

namespace ImGui
{
bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format)
{
    char text_buf[MAX_FORMAT_STR_LENGTH];
    bool value_changed = false;

    if (!display_format)
        display_format = "%.1f";
    snprintf(text_buf, MAX_FORMAT_STR_LENGTH, display_format, *v);

    if (ImGui::GetIO().WantTextInput)
    {
        value_changed = ImGui::SliderFloat(label, v, v_min, v_max, text_buf);

        int v_i = int(((*v - v_min) / v_step) + 0.5f);
        *v = v_min + (float)v_i * v_step;
    }
    else
    {
        // Map from [v_min,v_max] to [0,N]
        const int countValues = int((v_max - v_min) / v_step);
        int       v_i = int(((*v - v_min) / v_step) + 0.5f);
        value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

        // Remap from [0,N] to [v_min,v_max]
        *v = v_min + (float)v_i * v_step;
    }

    if (*v < v_min)
        *v = v_min;
    if (*v > v_max)
        *v = v_max;

    return value_changed;
}

bool SliderIntWithSteps(const char* label, int32_t* v, int32_t v_min, int32_t v_max, int32_t v_step, const char* display_format)
{
    char text_buf[MAX_FORMAT_STR_LENGTH];
    bool value_changed = false;

    if (!display_format)
        display_format = "%d";
    snprintf(text_buf, MAX_FORMAT_STR_LENGTH, display_format, *v);

    if (ImGui::GetIO().WantTextInput)
    {
        value_changed = ImGui::SliderInt(label, v, v_min, v_max, text_buf);

        int32_t v_i = int((*v - v_min) / v_step);
        *v = v_min + (int32_t)v_i * v_step;
    }
    else
    {
        // Map from [v_min,v_max] to [0,N]
        const int countValues = int((v_max - v_min) / v_step);
        int32_t   v_i = int((*v - v_min) / v_step);
        value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

        // Remap from [0,N] to [v_min,v_max]
        *v = v_min + (int32_t)v_i * v_step;
    }

    if (*v < v_min)
        *v = v_min;
    if (*v > v_max)
        *v = v_max;

    return value_changed;
}
} // namespace ImGui

/****************************************************************************/
// MARK: - Static Function Declarations
/****************************************************************************/

// UIWidget functions
static UIWidget* cloneWidget(const UIWidget* pWidget);
static void      processWidgetCallbacks(UIWidget* pWidget, bool deferred = false);
static void      processWidget(UIWidget* pWidget);
static void      destroyWidget(UIWidget* pWidget, bool freeUnderlying);

static void* alloc_func(size_t size, void* user_data)
{
    UNREF_PARAM(user_data);
    return tf_malloc(size);
}

static void dealloc_func(void* ptr, void* user_data)
{
    UNREF_PARAM(user_data);
    tf_free(ptr);
}

static void SetDefaultStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.4f;

    style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.46f, 0.47f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.70f, 0.70f, 0.70f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.70f, 0.70f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.50f, 0.52f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    style.Colors[ImGuiCol_Tab] = ImLerp(style.Colors[ImGuiCol_Header], style.Colors[ImGuiCol_TitleBg], 0.80f);
    style.Colors[ImGuiCol_TabHovered] = style.Colors[ImGuiCol_HeaderHovered];
    style.Colors[ImGuiCol_TabActive] = ImLerp(style.Colors[ImGuiCol_HeaderActive], style.Colors[ImGuiCol_TitleBg], 0.90f);
    style.Colors[ImGuiCol_TabUnfocused] = ImLerp(style.Colors[ImGuiCol_Tab], style.Colors[ImGuiCol_TitleBg], 0.80f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImLerp(
        ImLerp(style.Colors[ImGuiCol_HeaderActive], style.Colors[ImGuiCol_TitleBgActive], 0.80f), style.Colors[ImGuiCol_TitleBg], 0.40f);
    style.Colors[ImGuiCol_DockingPreview] = style.Colors[ImGuiCol_HeaderActive] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    style.ScaleAllSizes(min(dpiScale[0], dpiScale[1]));
}

#if defined(GFX_DRIVER_MEMORY_TRACKING)
void DrawDriverMemoryTrackingUI(void*)
{
    uint64_t totDriverMemory = GetDriverMemoryAmount();
    uint64_t totDriverAllocs = GetDriverAllocationsCount();

    struct DriverMemoryEntry
    {
        const char* name;
        uint64_t    memAmount;
        uint64_t    memAllocs;
    };

    if (totDriverMemory == 0)
    {
        return;
    }

    // Add UI components
    static bool driverMemUsageShowKB = false;
    ImGui::Checkbox("Driver Memory Show In Kilobytes", &driverMemUsageShowKB);
    const char* memUnit = driverMemUsageShowKB ? "KB" : "MB";
    size_t      memDivisor = driverMemUsageShowKB ? TF_KB : TF_MB;

    if (ImGui::TreeNodeEx("DriverMemoryTracking", ImGuiTreeNodeFlags_DefaultOpen, "Total Driver Memory Usage: %zu %s | Alloc Count: %zu",
                          (size_t)(totDriverMemory / memDivisor), memUnit, (size_t)totDriverAllocs))
    {
        // Gather all entries
        DriverMemoryEntry entries[TRACKED_OBJECT_TYPE_COUNT_MAX] = {};

        for (uint32_t i = 0; i < GetTrackedObjectTypeCount(); ++i)
        {
            entries[i].name = GetTrackedObjectName(i);
            entries[i].memAmount = GetDriverMemoryPerObject(i);
            entries[i].memAllocs = GetDriverAllocationsPerObject(i);
        }

        // Sort entries by amount of memory
        qsort(
            entries, GetTrackedObjectTypeCount(), sizeof(DriverMemoryEntry),
            +[](const void* lhs, const void* rhs)
            {
                DriverMemoryEntry* pLhs = (DriverMemoryEntry*)lhs;
                DriverMemoryEntry* pRhs = (DriverMemoryEntry*)rhs;
                if (pLhs->memAmount == pRhs->memAmount)
                    return (pLhs->memAllocs > pRhs->memAllocs) ? -1 : 1;

                return pLhs->memAmount > pRhs->memAmount ? -1 : 1;
            });

        const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("DriverMemoryTrackingTable", 3, tableFlags))
        {
            for (uint32_t type = 0; type < GetTrackedObjectTypeCount(); ++type)
            {
                float colors[2] = { 0.05f, 0.4f };
                ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(colors[type % 2]));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", entries[type].name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("Memory: %zu %s", (size_t)(entries[type].memAmount / memDivisor), memUnit);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Alloc count: %zu", (size_t)entries[type].memAllocs);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}
#endif // GFX_DRIVER_MEMORY_TRACKING

#if defined(GFX_DEVICE_MEMORY_TRACKING)
void DrawDeviceMemoryReportUI(void*)
{
    struct DeviceMemoryEntry
    {
        const char* name;
        uint64_t    memAmount;
        uint64_t    memAllocs;
    };

    const uint64_t memReportTotalMemory = GetDeviceMemoryAmount();
    const uint64_t memReportTotalAllocCount = GetDeviceAllocationsCount();

    if (memReportTotalMemory == 0)
    {
        return;
    }

    static bool memReportMemUsageShowKB = false;
    ImGui::Checkbox("Memory Report EXT Memory Show In Kilobytes", &memReportMemUsageShowKB);
    const char*  memUnit = memReportMemUsageShowKB ? "KB" : "MB";
    const size_t memDivisor = memReportMemUsageShowKB ? TF_KB : TF_MB;

    if (ImGui::TreeNodeEx("DeviceMemoryReport", ImGuiTreeNodeFlags_DefaultOpen, "Total GPU Memory: %zu %s | Alloc Count: %zu",
                          memReportTotalMemory / memDivisor, memUnit, memReportTotalAllocCount))
    {
        // Gather all entries
        DeviceMemoryEntry entries[TRACKED_OBJECT_TYPE_COUNT_MAX] = {};

        for (uint32_t i = 0; i < GetTrackedObjectTypeCount(); ++i)
        {
            entries[i].name = GetTrackedObjectName(i);
            entries[i].memAmount = GetDeviceMemoryPerObject(i);
            entries[i].memAllocs = GetDeviceAllocationsPerObject(i);
        }

        // Sort entries by amount of memory
        qsort(
            entries, GetTrackedObjectTypeCount(), sizeof(DeviceMemoryEntry),
            +[](const void* lhs, const void* rhs)
            {
                DeviceMemoryEntry* pLhs = (DeviceMemoryEntry*)lhs;
                DeviceMemoryEntry* pRhs = (DeviceMemoryEntry*)rhs;
                if (pLhs->memAmount == pRhs->memAmount)
                    return (pLhs->memAllocs > pRhs->memAllocs) ? -1 : 1;

                return pLhs->memAmount > pRhs->memAmount ? -1 : 1;
            });

        const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("DeviceMemoryTrackingTable", 3, tableFlags))
        {
            for (uint32_t type = 0; type < GetTrackedObjectTypeCount(); ++type)
            {
                float colors[2] = { 0.05f, 0.4f };
                ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(colors[type % 2]));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", entries[type].name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("Memory: %lu %s", entries[type].memAmount / memDivisor, memUnit);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Alloc count: %lu", entries[type].memAllocs);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}
#endif // GFX_DEVICE_MEMORY_TRACKING

/****************************************************************************/
// MARK: - Non-static Function Definitions
/****************************************************************************/

uint32_t addImguiFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, uint32_t fontID, float fontSize, uintptr_t* pFont)
{
    ImGuiIO& io = ImGui::GetIO();

    // Build and load the texture atlas into a texture
    int            width, height, bytesPerPixel;
    unsigned char* pixels = NULL;

    io.Fonts->ClearInputData();
    if (pFontBuffer == NULL)
    {
        *pFont = (uintptr_t)io.Fonts->AddFontDefault();
    }
    else
    {
        ImFontConfig config = {};
        config.FontDataOwnedByAtlas = false;
        ImFont* font = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize,
                                                      fontSize * min(pUserInterface->dpiScale[0], pUserInterface->dpiScale[1]), &config,
                                                      (const ImWchar*)pFontGlyphRanges);
        if (font != NULL)
        {
            io.FontDefault = font;
            *pFont = (uintptr_t)font;
        }
        else
        {
            *pFont = (uintptr_t)io.Fonts->AddFontDefault();
        }
    }

    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

    // At this point you've got the texture data and you need to upload that your your graphic system:
    // After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
    // This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
    Texture*        pTexture = NULL;
    SyncToken       token = {};
    TextureLoadDesc loadDesc = {};
    TextureDesc     textureDesc = {};
    textureDesc.arraySize = 1;
    textureDesc.depth = 1;
    textureDesc.descriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.format = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.height = height;
    textureDesc.mipLevels = 1;
    textureDesc.sampleCount = SAMPLE_COUNT_1;
    textureDesc.startState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.width = width;
    textureDesc.pName = "ImGui Font Texture";
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &pTexture;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    TextureUpdateDesc updateDesc = { pTexture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    for (uint32_t r = 0; r < subresource.rowCount; ++r)
    {
        memcpy(subresource.pMappedData + r * subresource.dstRowStride, pixels + r * subresource.srcRowStride, subresource.srcRowStride);
    }
    endUpdateResource(&updateDesc);

    UserInterface::UIFontResource newCachedFont = { pTexture, fontID, fontSize, *pFont };
    arrpush(pUserInterface->pCachedFontsArr, newCachedFont);

    ptrdiff_t fontTextureIndex = arrlen(pUserInterface->pCachedFontsArr) - 1;
    io.Fonts->TexID = (void*)fontTextureIndex;

    return (uint32_t)fontTextureIndex;
}

/****************************************************************************/
// MARK: - Static Value Definitions
/****************************************************************************/

static const uint64_t VERTEX_BUFFER_SIZE = FORGE_UI_MAX_VERTEXES * sizeof(ImDrawVert);
static const uint64_t INDEX_BUFFER_SIZE = FORGE_UI_MAX_INDEXES * sizeof(ImDrawIdx);

/****************************************************************************/
// MARK: - Base UIWidget Helper Functions
/****************************************************************************/

// CollapsingHeaderWidget private functions
static CollapsingHeaderWidget* cloneCollapsingHeaderWidget(const void* pWidget)
{
    const CollapsingHeaderWidget* pOriginalWidget = (const CollapsingHeaderWidget*)pWidget;
    CollapsingHeaderWidget*       pClonedWidget =
        (CollapsingHeaderWidget*)tf_malloc(sizeof(CollapsingHeaderWidget) + sizeof(UIWidget*) * pOriginalWidget->widgetsCount);

    pClonedWidget->pGroupedWidgets = (UIWidget**)(pClonedWidget + 1);
    pClonedWidget->widgetsCount = pOriginalWidget->widgetsCount;
    pClonedWidget->collapsed = pOriginalWidget->collapsed;
    pClonedWidget->previousCollapsed = false;
    pClonedWidget->defaultOpen = pOriginalWidget->defaultOpen;
    pClonedWidget->headerIsVisible = pOriginalWidget->headerIsVisible;
    for (uint32_t i = 0; i < pOriginalWidget->widgetsCount; ++i)
    {
        pClonedWidget->pGroupedWidgets[i] = cloneWidget(pOriginalWidget->pGroupedWidgets[i]);
    }

    return pClonedWidget;
}

// DebugTexturesWidget private functions
static DebugTexturesWidget* cloneDebugTexturesWidget(const void* pWidget)
{
    const DebugTexturesWidget* pOriginalWidget = (const DebugTexturesWidget*)pWidget;
    DebugTexturesWidget*       pClonedWidget = (DebugTexturesWidget*)tf_malloc(sizeof(DebugTexturesWidget));

    pClonedWidget->pTextures = pOriginalWidget->pTextures;
    pClonedWidget->texturesCount = pOriginalWidget->texturesCount;
    pClonedWidget->textureDisplaySize = pOriginalWidget->textureDisplaySize;

    return pClonedWidget;
}

// LabelWidget private functions
static LabelWidget* cloneLabelWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    LabelWidget* pClonedWidget = (LabelWidget*)tf_calloc(1, sizeof(LabelWidget));

    return pClonedWidget;
}

// ColorLabelWidget private functions
static ColorLabelWidget* cloneColorLabelWidget(const void* pWidget)
{
    const ColorLabelWidget* pOriginalWidget = (const ColorLabelWidget*)pWidget;
    ColorLabelWidget*       pClonedWidget = (ColorLabelWidget*)tf_calloc(1, sizeof(ColorLabelWidget));

    pClonedWidget->color = pOriginalWidget->color;

    return pClonedWidget;
}

// HorizontalSpaceWidget private functions
static HorizontalSpaceWidget* cloneHorizontalSpaceWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    HorizontalSpaceWidget* pClonedWidget = (HorizontalSpaceWidget*)tf_calloc(1, sizeof(HorizontalSpaceWidget));

    return pClonedWidget;
}

// SeparatorWidget private functions
static SeparatorWidget* cloneSeparatorWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    SeparatorWidget* pClonedWidget = (SeparatorWidget*)tf_calloc(1, sizeof(SeparatorWidget));

    return pClonedWidget;
}

// VerticalSeparatorWidget private functions
static VerticalSeparatorWidget* cloneVerticalSeparatorWidget(const void* pWidget)
{
    const VerticalSeparatorWidget* pOriginalWidget = (const VerticalSeparatorWidget*)pWidget;
    VerticalSeparatorWidget*       pClonedWidget = (VerticalSeparatorWidget*)tf_calloc(1, sizeof(VerticalSeparatorWidget));

    pClonedWidget->lineCount = pOriginalWidget->lineCount;

    return pClonedWidget;
}

// ButtonWidget private functions
static ButtonWidget* cloneButtonWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    ButtonWidget* pClonedWidget = (ButtonWidget*)tf_calloc(1, sizeof(ButtonWidget));

    return pClonedWidget;
}

// SliderFloatWidget private functions
static SliderFloatWidget* cloneSliderFloatWidget(const void* pWidget)
{
    const SliderFloatWidget* pOriginalWidget = (const SliderFloatWidget*)pWidget;
    SliderFloatWidget*       pClonedWidget = (SliderFloatWidget*)tf_calloc(1, sizeof(SliderFloatWidget));

    memset(pClonedWidget->format, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->format, pOriginalWidget->format);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->min = pOriginalWidget->min;
    pClonedWidget->max = pOriginalWidget->max;
    pClonedWidget->step = pOriginalWidget->step;

    return pClonedWidget;
}

// SliderFloat2Widget private functions
static SliderFloat2Widget* cloneSliderFloat2Widget(const void* pWidget)
{
    const SliderFloat2Widget* pOriginalWidget = (const SliderFloat2Widget*)pWidget;
    SliderFloat2Widget*       pClonedWidget = (SliderFloat2Widget*)tf_calloc(1, sizeof(SliderFloat2Widget));

    memset(pClonedWidget->format, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->format, pOriginalWidget->format);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->min = pOriginalWidget->min;
    pClonedWidget->max = pOriginalWidget->max;
    pClonedWidget->step = pOriginalWidget->step;

    return pClonedWidget;
}

// SliderFloat3Widget private functions
static SliderFloat3Widget* cloneSliderFloat3Widget(const void* pWidget)
{
    const SliderFloat3Widget* pOriginalWidget = (const SliderFloat3Widget*)pWidget;
    SliderFloat3Widget*       pClonedWidget = (SliderFloat3Widget*)tf_calloc(1, sizeof(SliderFloat3Widget));

    memset(pClonedWidget->format, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->format, pOriginalWidget->format);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->min = pOriginalWidget->min;
    pClonedWidget->max = pOriginalWidget->max;
    pClonedWidget->step = pOriginalWidget->step;

    return pClonedWidget;
}

// SliderFloat4Widget private functions
static SliderFloat4Widget* cloneSliderFloat4Widget(const void* pWidget)
{
    const SliderFloat4Widget* pOriginalWidget = (const SliderFloat4Widget*)pWidget;
    SliderFloat4Widget*       pClonedWidget = (SliderFloat4Widget*)tf_calloc(1, sizeof(SliderFloat4Widget));

    memset(pClonedWidget->format, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->format, pOriginalWidget->format);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->min = pOriginalWidget->min;
    pClonedWidget->max = pOriginalWidget->max;
    pClonedWidget->step = pOriginalWidget->step;

    return pClonedWidget;
}

// SliderIntWidget private functions
static SliderIntWidget* cloneSliderIntWidget(const void* pWidget)
{
    const SliderIntWidget* pOriginalWidget = (const SliderIntWidget*)pWidget;
    SliderIntWidget*       pClonedWidget = (SliderIntWidget*)tf_calloc(1, sizeof(SliderIntWidget));

    memset(pClonedWidget->format, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->format, pOriginalWidget->format);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->min = pOriginalWidget->min;
    pClonedWidget->max = pOriginalWidget->max;
    pClonedWidget->step = pOriginalWidget->step;

    return pClonedWidget;
}

// SliderUintWidget private functions
static SliderUintWidget* cloneSliderUintWidget(const void* pWidget)
{
    const SliderUintWidget* pOriginalWidget = (const SliderUintWidget*)pWidget;
    SliderUintWidget*       pClonedWidget = (SliderUintWidget*)tf_calloc(1, sizeof(SliderUintWidget));

    memset(pClonedWidget->format, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->format, pOriginalWidget->format);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->min = pOriginalWidget->min;
    pClonedWidget->max = pOriginalWidget->max;
    pClonedWidget->step = pOriginalWidget->step;

    return pClonedWidget;
}

// RadioButtonWidget private functions
static RadioButtonWidget* cloneRadioButtonWidget(const void* pWidget)
{
    const RadioButtonWidget* pOriginalWidget = (const RadioButtonWidget*)pWidget;
    RadioButtonWidget*       pClonedWidget = (RadioButtonWidget*)tf_calloc(1, sizeof(RadioButtonWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->radioId = pOriginalWidget->radioId;

    return pClonedWidget;
}

// CheckboxWidget private functions
static CheckboxWidget* cloneCheckboxWidget(const void* pWidget)
{
    const CheckboxWidget* pOriginalWidget = (const CheckboxWidget*)pWidget;
    CheckboxWidget*       pClonedWidget = (CheckboxWidget*)tf_calloc(1, sizeof(CheckboxWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// OneLineCheckboxWidget private functions
static OneLineCheckboxWidget* cloneOneLineCheckboxWidget(const void* pWidget)
{
    const OneLineCheckboxWidget* pOriginalWidget = (const OneLineCheckboxWidget*)pWidget;
    OneLineCheckboxWidget*       pClonedWidget = (OneLineCheckboxWidget*)tf_calloc(1, sizeof(OneLineCheckboxWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->color = pOriginalWidget->color;

    return pClonedWidget;
}

// CursorLocationWidget private functions
static CursorLocationWidget* cloneCursorLocationWidget(const void* pWidget)
{
    const CursorLocationWidget* pOriginalWidget = (const CursorLocationWidget*)pWidget;
    CursorLocationWidget*       pClonedWidget = (CursorLocationWidget*)tf_calloc(1, sizeof(CursorLocationWidget));

    pClonedWidget->location = pOriginalWidget->location;

    return pClonedWidget;
}

// DropdownWidget private functions
static DropdownWidget* cloneDropdownWidget(const void* pWidget)
{
    const DropdownWidget* pOriginalWidget = (const DropdownWidget*)pWidget;

    DropdownWidget* pClonedWidget = (DropdownWidget*)tf_malloc(sizeof(DropdownWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->pNames = pOriginalWidget->pNames;
    pClonedWidget->count = pOriginalWidget->count;

    return pClonedWidget;
}

// ColumnWidget private functions
static ColumnWidget* cloneColumnWidget(const void* pWidget)
{
    const ColumnWidget* pOriginalWidget = (const ColumnWidget*)pWidget;
    ColumnWidget*       pClonedWidget = (ColumnWidget*)tf_malloc(sizeof(ColumnWidget) + sizeof(UIWidget*) * pOriginalWidget->widgetsCount);
    pClonedWidget->pPerColumnWidgets = (UIWidget**)(pClonedWidget + 1);
    pClonedWidget->widgetsCount = pOriginalWidget->widgetsCount;

    for (uint32_t i = 0; i < pOriginalWidget->widgetsCount; ++i)
        pClonedWidget->pPerColumnWidgets[i] = cloneWidget(pOriginalWidget->pPerColumnWidgets[i]);

    return pClonedWidget;
}

// ProgressBarWidget private functions
static ProgressBarWidget* cloneProgressBarWidget(const void* pWidget)
{
    const ProgressBarWidget* pOriginalWidget = (const ProgressBarWidget*)pWidget;
    ProgressBarWidget*       pClonedWidget = (ProgressBarWidget*)tf_calloc(1, sizeof(ProgressBarWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->maxProgress = pOriginalWidget->maxProgress;

    return pClonedWidget;
}

// ColorSliderWidget private functions
static ColorSliderWidget* cloneColorSliderWidget(const void* pWidget)
{
    const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)pWidget;
    ColorSliderWidget*       pClonedWidget = (ColorSliderWidget*)tf_calloc(1, sizeof(ColorSliderWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// HistogramWidget private functions
static HistogramWidget* cloneHistogramWidget(const void* pWidget)
{
    const HistogramWidget* pOriginalWidget = (const HistogramWidget*)pWidget;
    HistogramWidget*       pClonedWidget = (HistogramWidget*)tf_calloc(1, sizeof(HistogramWidget));

    pClonedWidget->pValues = pOriginalWidget->pValues;
    pClonedWidget->count = pOriginalWidget->count;
    pClonedWidget->minScale = pOriginalWidget->minScale;
    pClonedWidget->maxScale = pOriginalWidget->maxScale;
    pClonedWidget->histogramSize = pOriginalWidget->histogramSize;
    pClonedWidget->histogramTitle = pOriginalWidget->histogramTitle;

    return pClonedWidget;
}

// PlotLinesWidget private functions
static PlotLinesWidget* clonePlotLinesWidget(const void* pWidget)
{
    const PlotLinesWidget* pOriginalWidget = (const PlotLinesWidget*)pWidget;
    PlotLinesWidget*       pClonedWidget = (PlotLinesWidget*)tf_calloc(1, sizeof(PlotLinesWidget));

    pClonedWidget->values = pOriginalWidget->values;
    pClonedWidget->numValues = pOriginalWidget->numValues;
    pClonedWidget->scaleMin = pOriginalWidget->scaleMin;
    pClonedWidget->scaleMax = pOriginalWidget->scaleMax;
    pClonedWidget->plotScale = pOriginalWidget->plotScale;
    pClonedWidget->title = pOriginalWidget->title;

    return pClonedWidget;
}

// ColorPickerWidget private functions
static ColorPickerWidget* cloneColorPickerWidget(const void* pWidget)
{
    const ColorPickerWidget* pOriginalWidget = (const ColorPickerWidget*)pWidget;
    ColorPickerWidget*       pClonedWidget = (ColorPickerWidget*)tf_calloc(1, sizeof(ColorPickerWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// ColorPickerWidget private functions
static Color3PickerWidget* cloneColor3PickerWidget(const void* pWidget)
{
    const Color3PickerWidget* pOriginalWidget = (const Color3PickerWidget*)pWidget;
    Color3PickerWidget*       pClonedWidget = (Color3PickerWidget*)tf_calloc(1, sizeof(Color3PickerWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// TextboxWidget private functions
static TextboxWidget* cloneTextboxWidget(const void* pWidget)
{
    const TextboxWidget* pOriginalWidget = (const TextboxWidget*)pWidget;

    TextboxWidget* pClonedWidget = (TextboxWidget*)tf_malloc(sizeof(TextboxWidget));

    pClonedWidget->pText = pOriginalWidget->pText;
    pClonedWidget->flags = pOriginalWidget->flags;
    pClonedWidget->pCallback = pOriginalWidget->pCallback;

    return pClonedWidget;
}

// DynamicTextWidget private functions
static DynamicTextWidget* cloneDynamicTextWidget(const void* pWidget)
{
    const DynamicTextWidget* pOriginalWidget = (const DynamicTextWidget*)pWidget;

    DynamicTextWidget* pClonedWidget = (DynamicTextWidget*)tf_malloc(sizeof(DynamicTextWidget));

    pClonedWidget->pText = pOriginalWidget->pText;
    pClonedWidget->pColor = pOriginalWidget->pColor;

    return pClonedWidget;
}

// FilledRectWidget private functions
static FilledRectWidget* cloneFilledRectWidget(const void* pWidget)
{
    const FilledRectWidget* pOriginalWidget = (const FilledRectWidget*)pWidget;
    FilledRectWidget*       pClonedWidget = (FilledRectWidget*)tf_calloc(1, sizeof(FilledRectWidget));

    pClonedWidget->pos = pOriginalWidget->pos;
    pClonedWidget->scale = pOriginalWidget->scale;
    pClonedWidget->color = pOriginalWidget->color;

    return pClonedWidget;
}

// DrawTextWidget private functions
static DrawTextWidget* cloneDrawTextWidget(const void* pWidget)
{
    const DrawTextWidget* pOriginalWidget = (const DrawTextWidget*)pWidget;
    DrawTextWidget*       pClonedWidget = (DrawTextWidget*)tf_calloc(1, sizeof(DrawTextWidget));

    pClonedWidget->pos = pOriginalWidget->pos;
    pClonedWidget->color = pOriginalWidget->color;

    return pClonedWidget;
}

// DrawTooltipWidget private functions
static DrawTooltipWidget* cloneDrawTooltipWidget(const void* pWidget)
{
    const DrawTooltipWidget* pOriginalWidget = (const DrawTooltipWidget*)pWidget;
    DrawTooltipWidget*       pClonedWidget = (DrawTooltipWidget*)tf_calloc(1, sizeof(DrawTooltipWidget));

    pClonedWidget->showTooltip = pOriginalWidget->showTooltip;
    pClonedWidget->text = pOriginalWidget->text;

    return pClonedWidget;
}

// DrawLineWidget private functions
static DrawLineWidget* cloneDrawLineWidget(const void* pWidget)
{
    const DrawLineWidget* pOriginalWidget = (const DrawLineWidget*)pWidget;
    DrawLineWidget*       pClonedWidget = (DrawLineWidget*)tf_calloc(1, sizeof(DrawLineWidget));

    pClonedWidget->pos1 = pOriginalWidget->pos1;
    pClonedWidget->pos2 = pOriginalWidget->pos2;
    pClonedWidget->color = pOriginalWidget->color;
    pClonedWidget->addItem = pOriginalWidget->addItem;

    return pClonedWidget;
}

// DrawCurveWidget private functions
static DrawCurveWidget* cloneDrawCurveWidget(const void* pWidget)
{
    const DrawCurveWidget* pOriginalWidget = (const DrawCurveWidget*)pWidget;
    DrawCurveWidget*       pClonedWidget = (DrawCurveWidget*)tf_calloc(1, sizeof(DrawCurveWidget));

    pClonedWidget->pos = pOriginalWidget->pos;
    pClonedWidget->numPoints = pOriginalWidget->numPoints;
    pClonedWidget->thickness = pOriginalWidget->thickness;
    pClonedWidget->color = pOriginalWidget->color;

    return pClonedWidget;
}

// CustomWidget private functions
static CustomWidget* cloneCustomWidget(const void* pWidget)
{
    const CustomWidget* pOriginalWidget = (const CustomWidget*)pWidget;
    CustomWidget*       pClonedWidget = (CustomWidget*)tf_calloc(1, sizeof(CustomWidget));

    pClonedWidget->pCallback = pOriginalWidget->pCallback;
    pClonedWidget->pUserData = pOriginalWidget->pUserData;
    pClonedWidget->pDestroyCallback = pOriginalWidget->pDestroyCallback;

    return pClonedWidget;
}

// UIWidget private functions
static void cloneWidgetBase(UIWidget* pDstWidget, const UIWidget* pSrcWidget)
{
    pDstWidget->type = pSrcWidget->type;
    strcpy(pDstWidget->label, pSrcWidget->label);

    pDstWidget->pOnHoverUserData = pSrcWidget->pOnHoverUserData;
    pDstWidget->pOnHover = pSrcWidget->pOnHover;
    pDstWidget->pOnActiveUserData = pSrcWidget->pOnActiveUserData;
    pDstWidget->pOnActive = pSrcWidget->pOnActive;
    pDstWidget->pOnFocusUserData = pSrcWidget->pOnFocusUserData;
    pDstWidget->pOnFocus = pSrcWidget->pOnFocus;
    pDstWidget->pOnEditedUserData = pSrcWidget->pOnEditedUserData;
    pDstWidget->pOnEdited = pSrcWidget->pOnEdited;
    pDstWidget->pOnDeactivatedUserData = pSrcWidget->pOnDeactivatedUserData;
    pDstWidget->pOnDeactivated = pSrcWidget->pOnDeactivated;
    pDstWidget->pOnDeactivatedAfterEditUserData = pSrcWidget->pOnDeactivatedAfterEditUserData;
    pDstWidget->pOnDeactivatedAfterEdit = pSrcWidget->pOnDeactivatedAfterEdit;

    pDstWidget->deferred = pSrcWidget->deferred;
}

// UIWidget private functions
static UIWidget* cloneWidget(const UIWidget* pOtherWidget)
{
    UIWidget* pWidget = (UIWidget*)tf_calloc(1, sizeof(UIWidget));
    cloneWidgetBase(pWidget, pOtherWidget);

    switch (pOtherWidget->type)
    {
    case WIDGET_TYPE_COLLAPSING_HEADER:
    {
        pWidget->pWidget = cloneCollapsingHeaderWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DEBUG_TEXTURES:
    {
        pWidget->pWidget = cloneDebugTexturesWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_LABEL:
    {
        pWidget->pWidget = cloneLabelWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_LABEL:
    {
        pWidget->pWidget = cloneColorLabelWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_HORIZONTAL_SPACE:
    {
        pWidget->pWidget = cloneHorizontalSpaceWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SEPARATOR:
    {
        pWidget->pWidget = cloneSeparatorWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_VERTICAL_SEPARATOR:
    {
        pWidget->pWidget = cloneVerticalSeparatorWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_BUTTON:
    {
        pWidget->pWidget = cloneButtonWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT:
    {
        pWidget->pWidget = cloneSliderFloatWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT2:
    {
        pWidget->pWidget = cloneSliderFloat2Widget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT3:
    {
        pWidget->pWidget = cloneSliderFloat3Widget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT4:
    {
        pWidget->pWidget = cloneSliderFloat4Widget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_INT:
    {
        pWidget->pWidget = cloneSliderIntWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_UINT:
    {
        pWidget->pWidget = cloneSliderUintWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_RADIO_BUTTON:
    {
        pWidget->pWidget = cloneRadioButtonWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_CHECKBOX:
    {
        pWidget->pWidget = cloneCheckboxWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_ONE_LINE_CHECKBOX:
    {
        pWidget->pWidget = cloneOneLineCheckboxWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_CURSOR_LOCATION:
    {
        pWidget->pWidget = cloneCursorLocationWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DROPDOWN:
    {
        pWidget->pWidget = cloneDropdownWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLUMN:
    {
        pWidget->pWidget = cloneColumnWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_PROGRESS_BAR:
    {
        pWidget->pWidget = cloneProgressBarWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_SLIDER:
    {
        pWidget->pWidget = cloneColorSliderWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_HISTOGRAM:
    {
        pWidget->pWidget = cloneHistogramWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_PLOT_LINES:
    {
        pWidget->pWidget = clonePlotLinesWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_PICKER:
    {
        pWidget->pWidget = cloneColorPickerWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR3_PICKER:
    {
        pWidget->pWidget = cloneColor3PickerWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_TEXTBOX:
    {
        pWidget->pWidget = cloneTextboxWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DYNAMIC_TEXT:
    {
        pWidget->pWidget = cloneDynamicTextWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_FILLED_RECT:
    {
        pWidget->pWidget = cloneFilledRectWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TEXT:
    {
        pWidget->pWidget = cloneDrawTextWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TOOLTIP:
    {
        pWidget->pWidget = cloneDrawTooltipWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_LINE:
    {
        pWidget->pWidget = cloneDrawLineWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_CURVE:
    {
        pWidget->pWidget = cloneDrawCurveWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_CUSTOM:
    {
        pWidget->pWidget = cloneCustomWidget(pOtherWidget->pWidget);
        break;
    }

    default:
        ASSERT(0);
    }

    return pWidget;
}

// UIWidget public functions
static void processWidgetCallbacks(UIWidget* pWidget, bool deferred)
{
    if (!deferred)
    {
        pWidget->hovered = ImGui::IsItemHovered();
        pWidget->active = ImGui::IsItemActive();
        pWidget->focused = ImGui::IsItemFocused();

        // ImGui::Button doesn't set the IsItemEdited flag, we assing ourselves:
        // pWidget->edited = ImGui::Button(...);
        if (pWidget->type != WIDGET_TYPE_BUTTON)
            pWidget->edited = ImGui::IsItemEdited();

        pWidget->deactivated = ImGui::IsItemDeactivated();
        pWidget->deactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
    }

    if (pWidget->deferred != deferred)
    {
        return;
    }

    if (pWidget->pOnHover && pWidget->hovered)
        pWidget->pOnHover(pWidget->pOnHoverUserData);

    if (pWidget->pOnActive && pWidget->active)
        pWidget->pOnActive(pWidget->pOnActiveUserData);

    if (pWidget->pOnFocus && pWidget->focused)
        pWidget->pOnFocus(pWidget->pOnFocusUserData);

    if (pWidget->pOnEdited && pWidget->edited)
        pWidget->pOnEdited(pWidget->pOnEditedUserData);

    if (pWidget->pOnDeactivated && pWidget->deactivated)
        pWidget->pOnDeactivated(pWidget->pOnDeactivatedUserData);

    if (pWidget->pOnDeactivatedAfterEdit && pWidget->deactivatedAfterEdit)
        pWidget->pOnDeactivatedAfterEdit(pWidget->pOnDeactivatedAfterEditUserData);
}

// CollapsingHeaderWidget private functions
static void processCollapsingHeaderWidget(UIWidget* pWidget)
{
    CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);

    if (pOriginalWidget->previousCollapsed != pOriginalWidget->collapsed)
    {
        ImGui::SetNextItemOpen(pOriginalWidget->collapsed);
        pOriginalWidget->previousCollapsed = pOriginalWidget->collapsed;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
    if (pOriginalWidget->defaultOpen)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->label, pOriginalWidget);

    if (!pOriginalWidget->headerIsVisible || ImGui::CollapsingHeader((const char*)label.data, flags))
    {
        for (uint32_t i = 0; i < pOriginalWidget->widgetsCount; ++i)
        {
            UIWidget* widget = pOriginalWidget->pGroupedWidgets[i];
            processWidget(widget);
        }
    }

    processWidgetCallbacks(pWidget);
    bdestroy(&label);
}

// DebugTexturesWidget private functions
static void processDebugTexturesWidget(UIWidget* pWidget)
{
    DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);

    for (uint32_t i = 0; i < pOriginalWidget->texturesCount; ++i)
    {
        Texture*  texture = (Texture*)pOriginalWidget->pTextures[i];
        ptrdiff_t id = pUserInterface->maxUIFonts + ((ptrdiff_t)pUserInterface->frameIdx * pUserInterface->maxDynamicUIUpdatesPerBatch +
                                                      pUserInterface->dynamicTexturesCount++);
        hmput(pUserInterface->pTextureHashmap, id, texture);
        ImGui::Image((void*)id, pOriginalWidget->textureDisplaySize);
        ImGui::SameLine();
    }

    processWidgetCallbacks(pWidget);
}

// LabelWidget private functions
static void processLabelWidget(UIWidget* pWidget)
{
    ImGui::Text("%s", pWidget->label);
    processWidgetCallbacks(pWidget);
}

// ColorLabelWidget private functions
static void processColorLabelWidget(UIWidget* pWidget)
{
    ColorLabelWidget* pOriginalWidget = (ColorLabelWidget*)(pWidget->pWidget);

    ImGui::TextColored(pOriginalWidget->color, "%s", pWidget->label);
    processWidgetCallbacks(pWidget);
}

// HorizontalSpaceWidget private functions
static void processHorizontalSpaceWidget(UIWidget* pWidget)
{
    ImGui::SameLine();
    processWidgetCallbacks(pWidget);
}

// SeparatorWidget private functions
static void processSeparatorWidget(UIWidget* pWidget)
{
    ImGui::Separator();
    processWidgetCallbacks(pWidget);
}

// VerticalSeparatorWidget private functions
static void processVerticalSeparatorWidget(UIWidget* pWidget)
{
    VerticalSeparatorWidget* pOriginalWidget = (VerticalSeparatorWidget*)(pWidget->pWidget);

    for (uint32_t i = 0; i < pOriginalWidget->lineCount; ++i)
    {
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    }

    processWidgetCallbacks(pWidget);
}

// ButtonWidget private functions
static void processButtonWidget(UIWidget* pWidget)
{
    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    COMPILE_ASSERT(sizeof(pWidget->pOnEdited) == sizeof(void*));
    bformat(&label, "%s##%p", pWidget->label, (void*)pWidget->pOnEdited);
    ASSERT(!bownsdata(&label));

    if (pWidget->sameLine)
    {
        ImGui::SameLine();
    }

    pWidget->edited = ImGui::Button((const char*)label.data);
    processWidgetCallbacks(pWidget);
}

// SliderFloatWidget private functions
static void processSliderFloatWidget(UIWidget* pWidget)
{
    SliderFloatWidget* pOriginalWidget = (SliderFloatWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->label);
    ImGui::SliderFloatWithSteps(label, pOriginalWidget->pData, pOriginalWidget->min, pOriginalWidget->max, pOriginalWidget->step,
                                pOriginalWidget->format);
    processWidgetCallbacks(pWidget);
}

// SliderFloat2Widget private functions
static void processSliderFloat2Widget(UIWidget* pWidget)
{
    SliderFloat2Widget* pOriginalWidget = (SliderFloat2Widget*)(pWidget->pWidget);

    ImGui::Text("%s", pWidget->label);
    for (uint32_t i = 0; i < 2; ++i)
    {
        char label[MAX_LABEL_LENGTH];
        LABELID1(&pOriginalWidget->pData->operator[](i), label);

        ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->min[i], pOriginalWidget->max[i],
                                    pOriginalWidget->step[i], pOriginalWidget->format);
        processWidgetCallbacks(pWidget);
    }
}

// SliderFloat3Widget private functions
static void processSliderFloat3Widget(UIWidget* pWidget)
{
    SliderFloat3Widget* pOriginalWidget = (SliderFloat3Widget*)(pWidget->pWidget);

    ImGui::Text("%s", pWidget->label);
    for (uint32_t i = 0; i < 3; ++i)
    {
        char label[MAX_LABEL_LENGTH];
        LABELID1(&pOriginalWidget->pData->operator[](i), label);
        ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->min[i], pOriginalWidget->max[i],
                                    pOriginalWidget->step[i], pOriginalWidget->format);
        processWidgetCallbacks(pWidget);
    }
}

// SliderFloat4Widget private functions
static void processSliderFloat4Widget(UIWidget* pWidget)
{
    SliderFloat4Widget* pOriginalWidget = (SliderFloat4Widget*)(pWidget->pWidget);

    ImGui::Text("%s", pWidget->label);
    for (uint32_t i = 0; i < 4; ++i)
    {
        char label[MAX_LABEL_LENGTH];
        LABELID1(&pOriginalWidget->pData->operator[](i), label);
        ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->min[i], pOriginalWidget->max[i],
                                    pOriginalWidget->step[i], pOriginalWidget->format);
        processWidgetCallbacks(pWidget);
    }
}

// SliderIntWidget private functions
static void processSliderIntWidget(UIWidget* pWidget)
{
    SliderIntWidget* pOriginalWidget = (SliderIntWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->label);
    ImGui::SliderIntWithSteps(label, pOriginalWidget->pData, pOriginalWidget->min, pOriginalWidget->max, pOriginalWidget->step,
                              pOriginalWidget->format);
    processWidgetCallbacks(pWidget);
}

// SliderUintWidget private functions
static void processSliderUintWidget(UIWidget* pWidget)
{
    SliderUintWidget* pOriginalWidget = (SliderUintWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->label);
    ImGui::SliderIntWithSteps(label, (int32_t*)pOriginalWidget->pData, (int32_t)pOriginalWidget->min, (int32_t)pOriginalWidget->max,
                              (int32_t)pOriginalWidget->step, pOriginalWidget->format);
    processWidgetCallbacks(pWidget);
}

// RadioButtonWidget private functions
static void processRadioButtonWidget(UIWidget* pWidget)
{
    RadioButtonWidget* pOriginalWidget = (RadioButtonWidget*)(pWidget->pWidget);
    unsigned char      labelBuf[MAX_LABEL_STR_LENGTH];
    bstring            label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->label, pOriginalWidget->pData);
    ASSERT(!bownsdata(&label));

    ImGui::RadioButton((const char*)label.data, pOriginalWidget->pData, pOriginalWidget->radioId);
    processWidgetCallbacks(pWidget);
}

// CheckboxWidget private functions
static void processCheckboxWidget(UIWidget* pWidget)
{
    CheckboxWidget* pOriginalWidget = (CheckboxWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->label);
    ImGui::Checkbox(label, pOriginalWidget->pData);
    processWidgetCallbacks(pWidget);
}

// OneLineCheckboxWidget private functions
static void processOneLineCheckboxWidget(UIWidget* pWidget)
{
    OneLineCheckboxWidget* pOriginalWidget = (OneLineCheckboxWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Checkbox(label, pOriginalWidget->pData);
    ImGui::SameLine();
    ImGui::TextColored(pOriginalWidget->color, "%s", pWidget->label);
    processWidgetCallbacks(pWidget);
}

// CursorLocationWidget private functions
static void processCursorLocationWidget(UIWidget* pWidget)
{
    CursorLocationWidget* pOriginalWidget = (CursorLocationWidget*)(pWidget->pWidget);

    ImGui::SetCursorPos(pOriginalWidget->location);
    processWidgetCallbacks(pWidget);
}

// DropdownWidget private functions
static void processDropdownWidget(UIWidget* pWidget)
{
    DropdownWidget* pOriginalWidget = (DropdownWidget*)(pWidget->pWidget);

    if (pOriginalWidget->count == 0)
        return;

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    uint32_t* pCurrent = pOriginalWidget->pData;
    ImGui::Text("%s", pWidget->label);
    ASSERT(pOriginalWidget->pNames);

    if (ImGui::BeginCombo(label, pOriginalWidget->pNames[*pCurrent]))
    {
        for (uint32_t i = 0; i < pOriginalWidget->count; ++i)
        {
            bool isSelected = (*pCurrent == i);

            if (ImGui::Selectable(pOriginalWidget->pNames[i], isSelected))
            {
                uint32_t prevValue = *pCurrent;
                *pCurrent = i;

                // Note that callbacks are sketchy with BeginCombo/EndCombo, so we manually process them here
                if (pWidget->pOnEdited)
                    pWidget->pOnEdited(pWidget->pOnEditedUserData);

                if (*pCurrent != prevValue)
                {
                    if (pWidget->pOnDeactivatedAfterEdit)
                        pWidget->pOnDeactivatedAfterEdit(pWidget->pOnDeactivatedAfterEditUserData);
                }
            }

            // Set the default focus to the currently selected item
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

// ColumnWidget private functions
static void processColumnWidget(UIWidget* pWidget)
{
    ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);

    // Test a simple 4 col table.
    ImGui::BeginColumns(pWidget->label, (int)pOriginalWidget->widgetsCount,
                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoForceWithinWindow);

    for (uint32_t i = 0; i < pOriginalWidget->widgetsCount; ++i)
    {
        processWidget(pOriginalWidget->pPerColumnWidgets[i]);
        ImGui::NextColumn();
    }

    ImGui::EndColumns();

    processWidgetCallbacks(pWidget);
}

// ProgressBarWidget private functions
static void processProgressBarWidget(UIWidget* pWidget)
{
    ProgressBarWidget* pOriginalWidget = (ProgressBarWidget*)(pWidget->pWidget);

    size_t currProgress = *(pOriginalWidget->pData);
    ImGui::Text("%s", pWidget->label);
    ImGui::ProgressBar((float)currProgress / pOriginalWidget->maxProgress);
    processWidgetCallbacks(pWidget);
}

// ColorSliderWidget private functions
static void processColorSliderWidget(UIWidget* pWidget)
{
    ColorSliderWidget* pOriginalWidget = (ColorSliderWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    float4* combo_color = pOriginalWidget->pData;

    float col[4] = { combo_color->x, combo_color->y, combo_color->z, combo_color->w };
    ImGui::Text("%s", pWidget->label);
    if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaPreview))
    {
        if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z || col[3] != combo_color->w)
        {
            *combo_color = col;
        }
    }
    processWidgetCallbacks(pWidget);
}

// HistogramWidget private functions
static void processHistogramWidget(UIWidget* pWidget)
{
    HistogramWidget* pOriginalWidget = (HistogramWidget*)(pWidget->pWidget);

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->label, pOriginalWidget->pValues);
    ASSERT(!bownsdata(&label));

    ImGui::PlotHistogram((const char*)label.data, pOriginalWidget->pValues, pOriginalWidget->count, 0, pOriginalWidget->histogramTitle,
                         *(pOriginalWidget->minScale), *(pOriginalWidget->maxScale), pOriginalWidget->histogramSize);

    processWidgetCallbacks(pWidget);
}

// PlotLinesWidget private functions
void processPlotLinesWidget(UIWidget* pWidget)
{
    PlotLinesWidget* pOriginalWidget = (PlotLinesWidget*)(pWidget->pWidget);

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->label, pOriginalWidget->values);
    ASSERT(!bownsdata(&label));

    ImGui::PlotLines((const char*)label.data, pOriginalWidget->values, pOriginalWidget->numValues, 0, pOriginalWidget->title,
                     *(pOriginalWidget->scaleMin), *(pOriginalWidget->scaleMax), *(pOriginalWidget->plotScale));

    processWidgetCallbacks(pWidget);
}

// ColorPickerWidget private functions
void processColorPickerWidget(UIWidget* pWidget)
{
    ColorPickerWidget* pOriginalWidget = (ColorPickerWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    float4* combo_color = pOriginalWidget->pData;

    float col[4] = { combo_color->x, combo_color->y, combo_color->z, combo_color->w };
    ImGui::Text("%s", pWidget->label);
    if (ImGui::ColorPicker4(label, col, ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_Float))
    {
        if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z || col[3] != combo_color->w)
        {
            *combo_color = col;
        }
    }
    processWidgetCallbacks(pWidget);
}

void processColor3PickerWidget(UIWidget* pWidget)
{
    Color3PickerWidget* pOriginalWidget = (Color3PickerWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    float3* combo_color = pOriginalWidget->pData;

    float col[3] = { combo_color->x, combo_color->y, combo_color->z };
    ImGui::Text("%s", pWidget->label);
    if (ImGui::ColorPicker3(label, col, ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_Float))
    {
        if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z)
        {
            *combo_color = col;
        }
    }
    processWidgetCallbacks(pWidget);
}

static int TextboxCallbackFunc(ImGuiInputTextCallbackData* data)
{
    TextboxWidget* pWidget = (TextboxWidget*)data->UserData;

    if (data->EventFlag & ImGuiInputTextFlags_CallbackAlways)
    {
        if (pWidget->flags & UI_TEXT_ENABLE_RESIZE)
        {
            bstring* pStr = pWidget->pText;
            ASSERT(data->Buf == (const char*)pStr->data);
            int res = balloc(pStr, data->BufTextLen + 1);
            ASSERT(res == BSTR_OK);
            data->Buf = (char*)pStr->data;
            data->BufSize = bmlen(pStr);
        }

        if (pWidget->pCallback)
            pWidget->pCallback(ImGui::GetIO().KeysDown);
    }

    return 0;
}

// TextboxWidget private functions
void processTextboxWidget(UIWidget* pWidget)
{
    TextboxWidget* pOriginalWidget = (TextboxWidget*)(pWidget->pWidget);
    bstring*       pText = pOriginalWidget->pText;

    ASSERT(biscstr(pText));
    char label[MAX_LABEL_LENGTH];
    LABELID1((const char*)pText->data, label);

    uint32_t flags = 0;
    if (pOriginalWidget->flags & UI_TEXT_AUTOSELECT_ALL)
        flags |= ImGuiInputTextFlags_AutoSelectAll;
    if (pOriginalWidget->pCallback)
        flags |= ImGuiInputTextFlags_CallbackAlways;

    ImGui::Text("%s", pWidget->label);
    ImGui::InputText(label, (char*)pText->data, bmlen(pText), flags, TextboxCallbackFunc, pOriginalWidget);
    pText->slen = (int)strlen((const char*)pText->data);

    processWidgetCallbacks(pWidget);
}

// DynamicTextWidget private functions
void processDynamicTextWidget(UIWidget* pWidget)
{
    DynamicTextWidget* pOriginalWidget = (DynamicTextWidget*)(pWidget->pWidget);
    ASSERT(pOriginalWidget->pText && bconstisvalid(pOriginalWidget->pText));
    ImGui::TextColored(*(pOriginalWidget->pColor), "%s", pOriginalWidget->pText->data);
    processWidgetCallbacks(pWidget);
}

// FilledRectWidget private functions
void processFilledRectWidget(UIWidget* pWidget)
{
    FilledRectWidget* pOriginalWidget = (FilledRectWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float2       pos = window->Pos - window->Scroll + pOriginalWidget->pos;
    float2       pos2 = float2(pos.x + pOriginalWidget->scale.x, pos.y + pOriginalWidget->scale.y);

    ImGui::GetWindowDrawList()->AddRectFilled(pos, pos2, ImGui::GetColorU32(pOriginalWidget->color));

    processWidgetCallbacks(pWidget);
}

// DrawTextWidget private functions
void processDrawTextWidget(UIWidget* pWidget)
{
    DrawTextWidget* pOriginalWidget = (DrawTextWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float2       pos = window->Pos - window->Scroll + pOriginalWidget->pos;
    const float2 line_size = ImGui::CalcTextSize(pWidget->label);

    ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(pOriginalWidget->color), pWidget->label);

    ImRect bounding_box(pos, pos + line_size);
    ImGui::ItemSize(bounding_box);
    ImGui::ItemAdd(bounding_box, 0);

    processWidgetCallbacks(pWidget);
}

// DrawTooltipWidget private functions
void processDrawTooltipWidget(UIWidget* pWidget)
{
    DrawTooltipWidget* pOriginalWidget = (DrawTooltipWidget*)(pWidget->pWidget);

    if ((*(pOriginalWidget->showTooltip)) == true)
    {
        ImGui::BeginTooltip();

        ImGui::TextUnformatted(pOriginalWidget->text);

        ImGui::EndTooltip();
    }

    processWidgetCallbacks(pWidget);
}

// DrawLineWidget private functions
void processDrawLineWidget(UIWidget* pWidget)
{
    DrawLineWidget* pOriginalWidget = (DrawLineWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float2       pos1 = window->Pos - window->Scroll + pOriginalWidget->pos1;
    float2       pos2 = window->Pos - window->Scroll + pOriginalWidget->pos2;

    ImGui::GetWindowDrawList()->AddLine(pos1, pos2, ImGui::GetColorU32(pOriginalWidget->color));

    if (pOriginalWidget->addItem)
    {
        ImRect bounding_box(pos1, pos2);
        ImGui::ItemSize(bounding_box);
        ImGui::ItemAdd(bounding_box, 0);
    }

    processWidgetCallbacks(pWidget);
}

// DrawCurveWidget private functions
void processDrawCurveWidget(UIWidget* pWidget)
{
    DrawCurveWidget* pOriginalWidget = (DrawCurveWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();

    for (uint32_t i = 0; i < pOriginalWidget->numPoints - 1; i++)
    {
        float2 pos1 = window->Pos - window->Scroll + pOriginalWidget->pos[i];
        float2 pos2 = window->Pos - window->Scroll + pOriginalWidget->pos[i + 1];
        ImGui::GetWindowDrawList()->AddLine(pos1, pos2, ImGui::GetColorU32(pOriginalWidget->color), pOriginalWidget->thickness);
    }

    processWidgetCallbacks(pWidget);
}

// CustomWidget private functions
void processCustomWidget(UIWidget* pWidget)
{
    CustomWidget* pOriginalWidget = (CustomWidget*)(pWidget->pWidget);

    if (pOriginalWidget->pCallback)
        pOriginalWidget->pCallback(pOriginalWidget->pUserData);

    // Note that we do not process widget callbacks for custom widgets
}

void processWidget(UIWidget* pWidget)
{
    pWidget->displayPosition = ImGui::GetCursorScreenPos();

    switch (pWidget->type)
    {
    case WIDGET_TYPE_COLLAPSING_HEADER:
    {
        processCollapsingHeaderWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DEBUG_TEXTURES:
    {
        processDebugTexturesWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_LABEL:
    {
        processLabelWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_LABEL:
    {
        processColorLabelWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_HORIZONTAL_SPACE:
    {
        processHorizontalSpaceWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SEPARATOR:
    {
        processSeparatorWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_VERTICAL_SEPARATOR:
    {
        processVerticalSeparatorWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_BUTTON:
    {
        processButtonWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT:
    {
        processSliderFloatWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT2:
    {
        processSliderFloat2Widget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT3:
    {
        processSliderFloat3Widget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT4:
    {
        processSliderFloat4Widget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_INT:
    {
        processSliderIntWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_UINT:
    {
        processSliderUintWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_RADIO_BUTTON:
    {
        processRadioButtonWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_CHECKBOX:
    {
        processCheckboxWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_ONE_LINE_CHECKBOX:
    {
        processOneLineCheckboxWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_CURSOR_LOCATION:
    {
        processCursorLocationWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DROPDOWN:
    {
        processDropdownWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLUMN:
    {
        processColumnWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_PROGRESS_BAR:
    {
        processProgressBarWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_SLIDER:
    {
        processColorSliderWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_HISTOGRAM:
    {
        processHistogramWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_PLOT_LINES:
    {
        processPlotLinesWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_PICKER:
    {
        processColorPickerWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR3_PICKER:
    {
        processColor3PickerWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_TEXTBOX:
    {
        processTextboxWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DYNAMIC_TEXT:
    {
        processDynamicTextWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_FILLED_RECT:
    {
        processFilledRectWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TEXT:
    {
        processDrawTextWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TOOLTIP:
    {
        processDrawTooltipWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_LINE:
    {
        processDrawLineWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_CURVE:
    {
        processDrawCurveWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_CUSTOM:
    {
        processCustomWidget(pWidget);
        break;
    }

    default:
        ASSERT(0);
    }
}

void destroyWidget(UIWidget* pWidget, bool freeUnderlying)
{
    if (freeUnderlying)
    {
        switch (pWidget->type)
        {
        case WIDGET_TYPE_COLLAPSING_HEADER:
        {
            CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);
            for (uint32_t i = 0; i < pOriginalWidget->widgetsCount; ++i)
                destroyWidget(pOriginalWidget->pGroupedWidgets[i], true);
            break;
        }
        case WIDGET_TYPE_COLUMN:
        {
            ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);
            for (ptrdiff_t i = 0; i < pOriginalWidget->widgetsCount; ++i)
                destroyWidget(pOriginalWidget->pPerColumnWidgets[i], true);
            break;
        }

        case WIDGET_TYPE_DROPDOWN:
        case WIDGET_TYPE_DEBUG_TEXTURES:
        case WIDGET_TYPE_LABEL:
        case WIDGET_TYPE_COLOR_LABEL:
        case WIDGET_TYPE_HORIZONTAL_SPACE:
        case WIDGET_TYPE_SEPARATOR:
        case WIDGET_TYPE_VERTICAL_SEPARATOR:
        case WIDGET_TYPE_BUTTON:
        case WIDGET_TYPE_SLIDER_FLOAT:
        case WIDGET_TYPE_SLIDER_FLOAT2:
        case WIDGET_TYPE_SLIDER_FLOAT3:
        case WIDGET_TYPE_SLIDER_FLOAT4:
        case WIDGET_TYPE_SLIDER_INT:
        case WIDGET_TYPE_SLIDER_UINT:
        case WIDGET_TYPE_RADIO_BUTTON:
        case WIDGET_TYPE_CHECKBOX:
        case WIDGET_TYPE_ONE_LINE_CHECKBOX:
        case WIDGET_TYPE_CURSOR_LOCATION:
        case WIDGET_TYPE_PROGRESS_BAR:
        case WIDGET_TYPE_COLOR_SLIDER:
        case WIDGET_TYPE_HISTOGRAM:
        case WIDGET_TYPE_PLOT_LINES:
        case WIDGET_TYPE_COLOR_PICKER:
        case WIDGET_TYPE_COLOR3_PICKER:
        case WIDGET_TYPE_TEXTBOX:
        case WIDGET_TYPE_DYNAMIC_TEXT:
        case WIDGET_TYPE_FILLED_RECT:
        case WIDGET_TYPE_DRAW_TEXT:
        case WIDGET_TYPE_DRAW_TOOLTIP:
        case WIDGET_TYPE_DRAW_LINE:
        case WIDGET_TYPE_DRAW_CURVE:
        {
            break;
        }

        case WIDGET_TYPE_CUSTOM:
        {
            CustomWidget* pOriginalWidget = (CustomWidget*)(pWidget->pWidget);
            if (pOriginalWidget->pDestroyCallback)
                pOriginalWidget->pDestroyCallback(pOriginalWidget->pUserData);
            break;
        }

        default:
            ASSERT(0);
        }

        tf_free(pWidget->pWidget);
        pWidget->pWidget = NULL;
    }

    tf_free(pWidget);
    pWidget = NULL;
}

#endif // ENABLE_FORGE_UI

/****************************************************************************/
// MARK: - Dynamic UI Public Functions
/****************************************************************************/

UIWidget* uiCreateDynamicWidgets(DynamicUIWidgets* pDynamicUI, const char* pLabel, const void* pWidget, WidgetType type)
{
#ifdef ENABLE_FORGE_UI
    UIWidget widget{};
    widget.type = type;
    widget.pWidget = (void*)pWidget;
    strcpy(widget.label, pLabel);

    return arrpush(pDynamicUI->dynamicProperties, cloneWidget(&widget));
#else
    return NULL;
#endif
}

void uiShowDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pDynamicUI->dynamicProperties); ++i)
    {
        UIWidget* pWidget = pDynamicUI->dynamicProperties[i];
        UIWidget* pNewWidget = uiCreateComponentWidget(pGui, pWidget->label, pWidget->pWidget, pWidget->type, false);
        cloneWidgetBase(pNewWidget, pWidget);
    }
#endif
}

void uiHideDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pDynamicUI->dynamicProperties); i++)
    {
        // We should not erase the widgets in this for-loop, otherwise the IDs
        // in mDynamicPropHandles will not match once  UIComponent::widgets changes size.
        uiDestroyComponentWidget(pGui, pDynamicUI->dynamicProperties[i]);
    }
#endif
}

void uiDestroyDynamicWidgets(DynamicUIWidgets* pDynamicUI)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pDynamicUI->dynamicProperties); ++i)
    {
        destroyWidget(pDynamicUI->dynamicProperties[i], true);
    }

    arrfree(pDynamicUI->dynamicProperties);
#endif
}

/****************************************************************************/
// MARK: - UI Component Public Functions
/****************************************************************************/

void uiCreateComponent(const char* pTitle, const UIComponentDesc* pDesc, UIComponent** ppUIComponent)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(ppUIComponent);
    UIComponent* pComponent = (UIComponent*)(tf_calloc(1, sizeof(UIComponent)));
    pComponent->hasCloseButton = false;
    pComponent->flags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;
#if defined(TARGET_IOS) || defined(__ANDROID__)
    pComponent->flags |= GUI_COMPONENT_FLAGS_START_COLLAPSED;
#endif

#ifdef ENABLE_FORGE_FONTS
    // Functions not accessible via normal interface header
    extern void*    fntGetRawFontData(uint32_t fontID);
    extern uint32_t fntGetRawFontDataSize(uint32_t fontID);

    bool useDefaultFallbackFont = false;

    // Use Requested Forge Font
    void*    pFontBuffer = fntGetRawFontData(pDesc->fontID);
    uint32_t fontBufferSize = fntGetRawFontDataSize(pDesc->fontID);
    if (pFontBuffer)
    {
        // See if that specific font id and size is already in use, if so just reuse it
        ptrdiff_t cachedFontIndex = -1;
        for (ptrdiff_t i = 0; i < arrlen(pUserInterface->pCachedFontsArr); ++i)
        {
            if (pUserInterface->pCachedFontsArr[i].fontId == pDesc->fontID &&
                pUserInterface->pCachedFontsArr[i].fontSize == pDesc->fontSize)
            {
                cachedFontIndex = i;

                pComponent->pFont = pUserInterface->pCachedFontsArr[i].pFont;
                pComponent->fontTextureIndex = (uint32_t)i;

                break;
            }
        }

        if (cachedFontIndex == -1) // didn't find that font in the cache
        {
            // Ensure we don't pass max amount of fonts
            if (arrlen(pUserInterface->pCachedFontsArr) < pUserInterface->maxUIFonts)
            {
                pComponent->fontTextureIndex =
                    addImguiFont(pFontBuffer, fontBufferSize, NULL, pDesc->fontID, pDesc->fontSize, &pComponent->pFont);
            }
            else
            {
                LOGF(eWARNING, "uiCreateComponent() has reached fonts capacity.  Consider increasing 'maxUIFonts' when initializing the "
                               "user interface.");
                useDefaultFallbackFont = true;
            }
        }
    }
    else
    {
        LOGF(eWARNING, "uiCreateComponent() uses an unknown font id (%u).  Will fallback to default UI font.", pDesc->fontID);
        useDefaultFallbackFont = true;
    }
#else
    useDefaultFallbackFont = true;
#endif

    if (useDefaultFallbackFont)
    {
        pComponent->pFont = pUserInterface->pDefaultFallbackFont;
        pComponent->fontTextureIndex = 0;
    }

    pComponent->initialWindowRect = { pDesc->startPosition.getX(), pDesc->startPosition.getY(), pDesc->startSize.getX(),
                                       pDesc->startSize.getY() };

    pComponent->active = true;
    strcpy(pComponent->title, pTitle);
    pComponent->alpha = 1.0f;
    arrpush(pUserInterface->components, pComponent);

    *ppUIComponent = pComponent;
#endif
}

void uiDestroyComponent(UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pGui);

    uiDestroyAllComponentWidgets(pGui);

    ptrdiff_t componentIndex = 0;
    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->components); ++i)
    {
        UIComponent* pComponent = pUserInterface->components[i];
        if (pComponent == pGui)
            componentIndex = i;
    }

    if (componentIndex < arrlen(pUserInterface->components))
    {
        uiDestroyAllComponentWidgets(pGui);
        arrdel(pUserInterface->components, componentIndex);
        arrfree(pGui->widgets);
    }

    tf_free(pGui);
#endif
}

void uiSetComponentActive(UIComponent* pUIComponent, bool active)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUIComponent);
    pUIComponent->active = active;
#endif
}

/****************************************************************************/
// MARK: - Public UIWidget Add/Remove Functions
/****************************************************************************/

UIWidget* uiCreateComponentWidget(UIComponent* pGui, const char* pLabel, const void* pWidget, WidgetType type, bool clone /* = true*/)
{
#ifdef ENABLE_FORGE_UI
    UIWidget* pBaseWidget = (UIWidget*)tf_calloc(1, sizeof(UIWidget));
    pBaseWidget->type = type;
    pBaseWidget->pWidget = (void*)pWidget;
    strcpy(pBaseWidget->label, pLabel);

    arrpush(pGui->widgets, clone ? cloneWidget(pBaseWidget) : pBaseWidget);
    arrpush(pGui->widgetsClone, clone);

    if (clone)
        tf_free(pBaseWidget);

    return pGui->widgets[arrlen(pGui->widgets) - 1];
#else
    return NULL;
#endif
}

void uiDestroyComponentWidget(UIComponent* pGui, UIWidget* pWidget)
{
#ifdef ENABLE_FORGE_UI
    ptrdiff_t i;
    for (i = 0; i < arrlen(pGui->widgets); ++i)
    {
        if (pGui->widgets[i]->pWidget == pWidget->pWidget)
            break;
    }
    if (i < arrlen(pGui->widgets))
    {
        UIWidget* iterWidget = pGui->widgets[i];
        destroyWidget(iterWidget, pGui->widgetsClone[i]);
        arrdel(pGui->widgetsClone, i);
        arrdel(pGui->widgets, i);
    }
#endif
}

void uiDestroyAllComponentWidgets(UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pGui->widgets); ++i)
    {
        destroyWidget(pGui->widgets[i], pGui->widgetsClone[i]); //-V595
    }

    arrfree(pGui->widgets);
    arrfree(pGui->widgetsClone);
#endif
}

/****************************************************************************/
// MARK: - Safe Public Setter Functions
/****************************************************************************/

void uiSetComponentFlags(UIComponent* pGui, int32_t flags)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pGui);

    pGui->flags = flags;
#endif
}

void uiSetWidgetDeferred(UIWidget* pWidget, bool deferred)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->deferred = deferred;
#endif
}

void uiSetWidgetOnHoverCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnHoverUserData = pUserData;
    pWidget->pOnHover = callback;
#endif
}

void uiSetWidgetOnActiveCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnActiveUserData = pUserData;
    pWidget->pOnActive = callback;
#endif
}

void uiSetWidgetOnFocusCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnFocusUserData = pUserData;
    pWidget->pOnFocus = callback;
#endif
}

void uiSetWidgetOnEditedCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnEditedUserData = pUserData;
    pWidget->pOnEdited = callback;
#endif
}

void uiSetWidgetOnDeactivatedCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnDeactivatedUserData = pUserData;
    pWidget->pOnDeactivated = callback;
#endif
}

void uiSetWidgetOnDeactivatedAfterEditCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnDeactivatedAfterEditUserData = pUserData;
    pWidget->pOnDeactivatedAfterEdit = callback;
#endif
}

/// Set whether or not a given UI Widget should be on the sameline than the previous one
FORGE_API void uiSetSameLine(UIWidget* pGuiComponent, bool sameLine)
{
#ifdef ENABLE_FORGE_UI
    pGuiComponent->sameLine = sameLine;
#endif
}

void uiNewFrame()
{
#if defined(ENABLE_FORGE_REMOTE_UI)
    if (pUserInterface->enableRemoteUI && remoteAppIsConnected())
    {
        remoteAppReceiveInputData();
        if (remoteAppShouldSendFontTexture())
        {
            unsigned char* pPixelData = NULL;
            int            width = 0;
            int            height = 0;
            ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pPixelData, &width, &height);
            remoteControlSendTexture(TinyImageFormat_R8G8B8A8_SRGB, (uint64_t)ImGui::GetIO().Fonts->TexID, width, height,
                                     width * height * 4, pPixelData);
        }
    }
#endif
    pUserInterface->dynamicTexturesCount = 0;
    ImGui::NewFrame();
}

void uiEndFrame() { ImGui::EndFrame(); }

/****************************************************************************/
// MARK: - Private Platform Layer Life Cycle Functions
/****************************************************************************/

bool platformInitUserInterface()
{
#ifdef ENABLE_FORGE_UI
    UserInterface* pAppUI = tf_new(UserInterface);

    pAppUI->showDemoUiWindow = false;

    pAppUI->handledGestures = false;
    pAppUI->active = true;
    memset(pAppUI->postUpdateKeyDownStates, 0, sizeof(pAppUI->postUpdateKeyDownStates));

    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, pAppUI->dpiScale);

    //// init UI (input)
    ImGui::SetAllocatorFunctions(alloc_func, dealloc_func);
    pAppUI->context = ImGui::CreateContext();
    ImGui::SetCurrentContext(pAppUI->context);

    SetDefaultStyle();

    ImGuiIO& io = ImGui::GetIO();

    // TODO: Might be a good idea to considder adding these flags in some platforms:
    //         - ImGuiConfigFlags_NavEnableKeyboard
    io.ConfigFlags = ImGuiConfigFlags_NavEnableGamepad;

    // Tell ImGui that we support ImDrawCmd::VtxOffset, otherwise ImGui will always set it to 0
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    io.KeyMap[ImGuiKey_Tab] = UISystemInputActions::UI_ACTION_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = UISystemInputActions::UI_ACTION_KEY_LEFT_ARROW;
    io.KeyMap[ImGuiKey_RightArrow] = UISystemInputActions::UI_ACTION_KEY_RIGHT_ARROW;
    io.KeyMap[ImGuiKey_UpArrow] = UISystemInputActions::UI_ACTION_KEY_UP_ARROW;
    io.KeyMap[ImGuiKey_DownArrow] = UISystemInputActions::UI_ACTION_KEY_DOWN_ARROW;
    io.KeyMap[ImGuiKey_PageUp] = UISystemInputActions::UI_ACTION_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = UISystemInputActions::UI_ACTION_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = UISystemInputActions::UI_ACTION_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = UISystemInputActions::UI_ACTION_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = UISystemInputActions::UI_ACTION_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = UISystemInputActions::UI_ACTION_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = UISystemInputActions::UI_ACTION_KEY_BACK_SPACE;
    io.KeyMap[ImGuiKey_Space] = UISystemInputActions::UI_ACTION_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = UISystemInputActions::UI_ACTION_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = UISystemInputActions::UI_ACTION_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = UISystemInputActions::UI_ACTION_KEY_A;
    io.KeyMap[ImGuiKey_C] = UISystemInputActions::UI_ACTION_KEY_C;
    io.KeyMap[ImGuiKey_V] = UISystemInputActions::UI_ACTION_KEY_V;
    io.KeyMap[ImGuiKey_X] = UISystemInputActions::UI_ACTION_KEY_X;
    io.KeyMap[ImGuiKey_Y] = UISystemInputActions::UI_ACTION_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = UISystemInputActions::UI_ACTION_KEY_Z;

    pUserInterface = pAppUI;
#endif

    return true;
}

void platformExitUserInterface()
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->components); ++i)
    {
        uiDestroyAllComponentWidgets(pUserInterface->components[i]);
        tf_free(pUserInterface->components[i]);
    }
    arrfree(pUserInterface->components);

    ImGui::DestroyContext(pUserInterface->context);

    tf_delete(pUserInterface);
#endif
}

void platformUpdateUserInterface(float deltaTime)
{
#ifdef ENABLE_FORGE_UI
    // Render can me nullptr when initUserInterface wasn't called, this can happen when the build compiled using
    // ENABLED_FORGE_UI but then in runtime the App decided not to use the UI
    if (pUserInterface->pRenderer == nullptr)
        return;

    // (UIComponent*)[dyn_size]
    UIComponent** activeComponents = NULL;
    arrsetcap(activeComponents, arrlen(pUserInterface->components));

    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->components); ++i)
        if (pUserInterface->components[i]->active)
            arrpush(activeComponents, pUserInterface->components[i]);

    if (arrlen(activeComponents) == 0)
    {
        if (arrcap(activeComponents) > 0)
            arrfree(activeComponents);
    }

    GUIDriverUpdate guiUpdate = {};
    guiUpdate.pUIComponents = activeComponents;
    guiUpdate.componentCount = (uint32_t)arrlenu(activeComponents);
    guiUpdate.deltaTime = deltaTime;
    guiUpdate.width = pUserInterface->displayWidth;
    guiUpdate.height = pUserInterface->displayHeight;
    guiUpdate.showDemoWindow = pUserInterface->showDemoUiWindow;

    ImGui::SetCurrentContext(pUserInterface->context);
    // #TODO: Use window size as render-target size cannot be trusted to be the same as window size
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = pUserInterface->displayWidth;
    io.DisplaySize.y = pUserInterface->displayHeight;
    io.DisplayFramebufferScale.x = pUserInterface->width / pUserInterface->displayWidth;
    io.DisplayFramebufferScale.y = pUserInterface->height / pUserInterface->displayHeight;
    io.FontGlobalScale = min(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    io.DeltaTime = guiUpdate.deltaTime;
    if (pUserInterface->pMovePosition)
        io.MousePos = *pUserInterface->pMovePosition * io.DisplayFramebufferScale;

    // Gamepad connected
    {
        bool anyGamepadConnected = false;
        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            anyGamepadConnected |= gamePadConnected(i);

        if (anyGamepadConnected)
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        else
            io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    }

    memcpy(io.NavInputs, pUserInterface->navInputs, sizeof(pUserInterface->navInputs));

    uiNewFrame();

    if (pUserInterface->active)
    {
        if (guiUpdate.showDemoWindow)
            ImGui::ShowDemoWindow();

        pUserInterface->lastUpdateCount = guiUpdate.componentCount;

        for (uint32_t compIndex = 0; compIndex < guiUpdate.componentCount; ++compIndex)
        {
            if (!guiUpdate.pUIComponents)
                continue;

            UIComponent*          pComponent = guiUpdate.pUIComponents[compIndex];
            char                  title[MAX_TITLE_STR_LENGTH] = { 0 };
            int32_t               UIComponentFlags = pComponent->flags;
            bool*                 pCloseButtonActiveValue = pComponent->hasCloseButton ? &pComponent->hasCloseButton : NULL;
            const char* const*    contextualMenuLabels = pComponent->contextualMenuLabels;
            const WidgetCallback* contextualMenuCallbacks = pComponent->contextualMenuCallbacks;
            const size_t          contextualMenuCount = pComponent->contextualMenuCount;
            const float4*         pWindowRect = &pComponent->initialWindowRect;
            float4*               pCurrentWindowRect = &pComponent->currentWindowRect;
            UIWidget**            pProps = pComponent->widgets;
            ptrdiff_t             propCount = arrlen(pComponent->widgets);

            strcpy(title, pComponent->title);

            if (title[0] == '\0')
                snprintf(title, MAX_TITLE_STR_LENGTH, "##%llu", (unsigned long long)pComponent);
            // Setup the ImGuiWindowFlags
            ImGuiWindowFlags guiWinFlags = GUI_COMPONENT_FLAGS_NONE;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_TITLE_BAR)
                guiWinFlags |= ImGuiWindowFlags_NoTitleBar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE)
                guiWinFlags |= ImGuiWindowFlags_NoResize;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
                guiWinFlags |= ImGuiWindowFlags_NoMove;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_NoScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_COLLAPSE)
                guiWinFlags |= ImGuiWindowFlags_NoCollapse;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE)
                guiWinFlags |= ImGuiWindowFlags_AlwaysAutoResize;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_INPUTS)
                guiWinFlags |= ImGuiWindowFlags_NoInputs;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_MEMU_BAR)
                guiWinFlags |= ImGuiWindowFlags_MenuBar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_HORIZONTAL_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_HorizontalScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_FOCUS_ON_APPEARING)
                guiWinFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS)
                guiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_USE_WINDOW_PADDING)
                guiWinFlags |= ImGuiWindowFlags_AlwaysUseWindowPadding;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_INPUT)
                guiWinFlags |= ImGuiWindowFlags_NoNavInputs;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_FOCUS)
                guiWinFlags |= ImGuiWindowFlags_NoNavFocus;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_DOCKING)
                guiWinFlags |= ImGuiWindowFlags_NoDocking;

            ImGui::PushFont((ImFont*)pComponent->pFont);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pComponent->alpha);

            if (pComponent->pPreProcessCallback)
                pComponent->pPreProcessCallback(pComponent->pUserData);

            bool result = ImGui::Begin(title, pCloseButtonActiveValue, guiWinFlags);
            if (result)
            {
                // Setup the contextual menus
                if (contextualMenuCount != 0 && ImGui::BeginPopupContextItem()) // <-- This is using IsItemHovered()
                {
                    for (size_t i = 0; i < contextualMenuCount; i++)
                    {
                        if (ImGui::MenuItem(contextualMenuLabels[i]))
                        {
                            if (contextualMenuCallbacks && contextualMenuCallbacks[i])
                                contextualMenuCallbacks[i](pComponent->pUserData);
                        }
                    }
                    ImGui::EndPopup();
                }

                bool overrideSize = false;
                bool overridePos = false;

                if ((UIComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE) && !(UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE))
                    overrideSize = true;

                if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
                    overridePos = true;

                ImGui::SetWindowSize(float2(pWindowRect->z, pWindowRect->w), overrideSize ? ImGuiCond_Always : ImGuiCond_Once);
                ImGui::SetWindowPos(float2(pWindowRect->x, pWindowRect->y), overridePos ? ImGuiCond_Always : ImGuiCond_Once);

                if (UIComponentFlags & GUI_COMPONENT_FLAGS_START_COLLAPSED)
                    ImGui::SetWindowCollapsed(true, ImGuiCond_Once);

                for (ptrdiff_t i = 0; i < propCount; ++i)
                {
                    if (pProps[i] != nullptr)
                    {
                        processWidget(pProps[i]);
                    }
                }
            }

            float2 pos = ImGui::GetWindowPos();
            float2 size = ImGui::GetWindowSize();
            pCurrentWindowRect->x = pos.x;
            pCurrentWindowRect->y = pos.y;
            pCurrentWindowRect->z = size.x;
            pCurrentWindowRect->w = size.y;
            pUserInterface->lastUpdateMin[compIndex] = pos;
            pUserInterface->lastUpdateMax[compIndex] = pos + size;

            // Need to call ImGui::End event if result is false since we called ImGui::Begin
            ImGui::End();

            if (pComponent->pPostProcessCallback)
                pComponent->pPostProcessCallback(pComponent->pUserData);

            ImGui::PopStyleVar();
            ImGui::PopFont();
        }
    }

    uiEndFrame();

    if (pUserInterface->active)
    {
        for (uint32_t compIndex = 0; compIndex < guiUpdate.componentCount; ++compIndex)
        {
            if (!guiUpdate.pUIComponents)
                continue;

            UIComponent* pComponent = guiUpdate.pUIComponents[compIndex];
            UIWidget**   pProps = pComponent->widgets;
            ptrdiff_t    propCount = arrlen(pComponent->widgets);

            for (ptrdiff_t i = 0; i < propCount; ++i)
            {
                if (pProps[i] != nullptr)
                {
                    processWidgetCallbacks(pProps[i], true);
                }
            }
        }
    }

    if (!io.MouseDown[0])
    {
        io.MousePos = float2(-FLT_MAX);
    }

    pUserInterface->handledGestures = false;

    // Apply post update keydown states
    COMPILE_ASSERT(sizeof(pUserInterface->postUpdateKeyDownStates) == sizeof(io.KeysDown));
    memcpy(io.KeysDown, pUserInterface->postUpdateKeyDownStates, sizeof(io.KeysDown));

    arrfree(activeComponents);

    // extern void updateProfilerUI();
    // updateProfilerUI();
#endif
}

/****************************************************************************/
// MARK: - Private Static Reused Draw Functionalities
/****************************************************************************/
#if defined(ENABLE_FORGE_UI)
static void fillDrawInformation(UserInterfaceDrawData* pUIDrawData, ImDrawData* pImDrawData)
{
    pUIDrawData->displayPos = pImDrawData->DisplayPos;
    pUIDrawData->displaySize = pImDrawData->DisplaySize;
    pUIDrawData->vertexSize = sizeof(ImDrawVert);
    pUIDrawData->indexSize = sizeof(ImDrawIdx);

    uint32_t numVtx = 0;
    uint32_t numIdx = 0;
    for (int n = 0; n < pImDrawData->CmdListsCount; n++)
    {
        const ImDrawList* pCmdList = pImDrawData->CmdLists[n];
        numVtx += pCmdList->VtxBuffer.size();
        numIdx += pCmdList->IdxBuffer.size();
    }

    pUIDrawData->vertexCount = numVtx;
    pUIDrawData->indexCount = numIdx;

    pUIDrawData->numDrawCommands = 0;
    for (int n = 0; n < pImDrawData->CmdListsCount; n++)
    {
        const ImDrawList* pCmdList = pImDrawData->CmdLists[n];
        pUIDrawData->numDrawCommands += pCmdList->CmdBuffer.size();
    }
}

static void cmdPrepareRenderingForUI(Cmd* pCmd, const float2& displayPos, const float2& displaySize, Pipeline* pPipeline,
                                     const uint64_t vOffset, const uint64_t iOffset)
{
    const float                  L = displayPos.x;
    const float                  R = displayPos.x + displaySize.x;
    const float                  T = displayPos.y;
    const float                  B = displayPos.y + displaySize.y;
    alignas(alignof(mat4)) float mvp[4][4] = {
        { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.5f, 0.0f },
        { (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
    };

    BufferUpdateDesc update = { pUserInterface->pUniformBuffer[pUserInterface->frameIdx] };
    beginUpdateResource(&update);
    memcpy(update.pMappedData, &mvp, sizeof(mvp));
    endUpdateResource(&update);

    const uint32_t vertexStride = sizeof(ImDrawVert);

    cmdSetViewport(pCmd, 0.0f, 0.0f, displaySize.x, displaySize.y, 0.0f, 1.0f);
    cmdSetScissor(pCmd, (uint32_t)displayPos.x, (uint32_t)displayPos.y, (uint32_t)displaySize.x, (uint32_t)displaySize.y);

    cmdBindPipeline(pCmd, pPipeline);
    cmdBindIndexBuffer(pCmd, pUserInterface->pIndexBuffer, sizeof(ImDrawIdx) == sizeof(uint16_t) ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32,
                       iOffset);
    cmdBindVertexBuffer(pCmd, 1, &pUserInterface->pVertexBuffer, &vertexStride, &vOffset);
    cmdBindDescriptorSet(pCmd, pUserInterface->frameIdx, pUserInterface->pDescriptorSetUniforms);
}

static void cmdDrawUICommand(Cmd* pCmd, const UserInterfaceDrawCommand* pImDrawCmd, const float2& displayPos, const float2& displaySize,
                             Pipeline** ppPipelineInOut, Pipeline** ppPrevPipelineInOut, int32_t& globalVtxOffsetInOut,
                             int32_t& globalIdxOffsetInOut, uint32_t& prevSetIndexInOut)
{
    // for (uint32_t i = 0; i < (uint32_t)pCmdList->CmdBuffer.size(); i++)
    //{
    //	const ImDrawCmd* pImDrawCmd = &pCmdList->CmdBuffer[i];
    //	if (pImDrawCmd->UserCallback)
    //	{
    //		// User callback (registered via ImDrawList::AddCallback)
    //		pImDrawCmd->UserCallback(pCmdList, pImDrawCmd);
    //	}
    //	else
    //	{
    //  Clamp to viewport as cmdSetScissor() won't accept values that are off bounds
    float2 clipMin = { clamp(pImDrawCmd->clipRect.x - displayPos.x, 0.0f, displaySize.x),
                       clamp(pImDrawCmd->clipRect.y - displayPos.y, 0.0f, displaySize.y) };
    float2 clipMax = { clamp(pImDrawCmd->clipRect.z - displayPos.x, 0.0f, displaySize.x),
                       clamp(pImDrawCmd->clipRect.w - displayPos.y, 0.0f, displaySize.y) };
    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
    {
        return;
    }
    if (!pImDrawCmd->elemCount)
    {
        return;
    }

    uint2 offset = { (uint32_t)clipMin.x, (uint32_t)clipMin.y };
    uint2 ext = { (uint32_t)(clipMax.x - clipMin.x), (uint32_t)(clipMax.y - clipMin.y) };
    cmdSetScissor(pCmd, offset.x, offset.y, ext.x, ext.y);

    ptrdiff_t id = (ptrdiff_t)pImDrawCmd->textureId;
    uint32_t  setIndex = (uint32_t)id;
    if (id >= pUserInterface->maxUIFonts)
    {
        if (pUserInterface->dynamicTexturesCount >= pUserInterface->maxDynamicUIUpdatesPerBatch)
        {
            LOGF(eWARNING,
                 "Too many dynamic UIs.  Consider increasing 'maxDynamicUIUpdatesPerBatch' when initializing the user interface.");
            return;
        }
        Texture* tex = hmgetp(pUserInterface->pTextureHashmap, id)->value;

#if defined(ENABLE_FORGE_REMOTE_UI)
        // UI Remote Control still receives texture pointers as IDs
        if (tex == NULL)
        {
            tex = (Texture*)id;
            setIndex = (uint32_t)(pUserInterface->maxUIFonts + (pUserInterface->frameIdx * pUserInterface->maxDynamicUIUpdatesPerBatch +
                                                                 pUserInterface->dynamicTexturesCount++));
        }
#endif // ENABLE_FORGE_REMOTE_UI

        DescriptorData params[1] = {};
        params[0].pName = "uTex";
        params[0].ppTextures = &tex;
        updateDescriptorSet(pUserInterface->pRenderer, setIndex, pUserInterface->pDescriptorSetTexture, 1, params);

        uint32_t pipelineIndex = (uint32_t)log2(params[0].ppTextures[0]->sampleCount);
        *ppPipelineInOut = pUserInterface->pPipelineTextured[pipelineIndex];
    }
    else
    {
        *ppPipelineInOut = pUserInterface->pPipelineTextured[0];
    }

    if (*ppPrevPipelineInOut != *ppPipelineInOut)
    {
        cmdBindPipeline(pCmd, *ppPipelineInOut);
        *ppPrevPipelineInOut = *ppPipelineInOut;
    }

    if (setIndex != prevSetIndexInOut)
    {
        cmdBindDescriptorSet(pCmd, setIndex, pUserInterface->pDescriptorSetTexture);
        prevSetIndexInOut = setIndex;
    }

    cmdDrawIndexed(pCmd, pImDrawCmd->elemCount, pImDrawCmd->indexOffset + globalIdxOffsetInOut,
                   pImDrawCmd->vertexOffset + globalVtxOffsetInOut);
    globalIdxOffsetInOut += pImDrawCmd->indexCount;
    globalVtxOffsetInOut += pImDrawCmd->vertexCount;
}

#endif // ENABLE_FORGE_UI

/****************************************************************************/
// MARK: - Public App Layer Life Cycle Functions
/****************************************************************************/

void initUserInterface(UserInterfaceDesc* pDesc)
{
#ifdef ENABLE_FORGE_UI
    pUserInterface->pRenderer = pDesc->pRenderer;
    pUserInterface->pPipelineCache = pDesc->pCache;
    pUserInterface->maxDynamicUIUpdatesPerBatch = pDesc->maxDynamicUIUpdatesPerBatch;
    pUserInterface->maxUIFonts = pDesc->maxUIFonts + 1; // +1 to account for a default fallback font
    pUserInterface->frameCount = pDesc->frameCount;
    // Remote UI is intentionally parked while the runtime focuses on a single-client path.
    pUserInterface->enableRemoteUI = false;
    ASSERT(pUserInterface->frameCount <= MAX_FRAMES);
    /************************************************************************/
    // Rendering resources
    /************************************************************************/
    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(pUserInterface->pRenderer, &samplerDesc, &pUserInterface->pDefaultSampler);

    BufferLoadDesc vbDesc = {};
    vbDesc.desc.descriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.desc.memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.desc.size = VERTEX_BUFFER_SIZE * pDesc->frameCount;
    vbDesc.desc.flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.desc.pName = "UI Vertex Buffer";
    vbDesc.ppBuffer = &pUserInterface->pVertexBuffer;
    addResource(&vbDesc, NULL);

    BufferLoadDesc ibDesc = vbDesc;
    ibDesc.desc.descriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.desc.size = INDEX_BUFFER_SIZE * pDesc->frameCount;
    vbDesc.desc.pName = "UI Index Buffer";
    ibDesc.ppBuffer = &pUserInterface->pIndexBuffer;
    addResource(&ibDesc, NULL);

    BufferLoadDesc ubDesc = {};
    ubDesc.desc.descriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.desc.memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.desc.flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.desc.size = sizeof(mat4);
    vbDesc.desc.pName = "UI Uniform Buffer";
    for (uint32_t i = 0; i < pDesc->frameCount; ++i)
    {
        ubDesc.ppBuffer = &pUserInterface->pUniformBuffer[i];
        addResource(&ubDesc, NULL);
    }

    VertexLayout* vertexLayout = &pUserInterface->vertexLayoutTextured;
    vertexLayout->bindingCount = 1;
    vertexLayout->attribCount = 3;
    vertexLayout->attribs[0].semantic = SEMANTIC_POSITION;
    vertexLayout->attribs[0].format = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout->attribs[0].binding = 0;
    vertexLayout->attribs[0].location = 0;
    vertexLayout->attribs[0].offset = 0;
    vertexLayout->attribs[1].semantic = SEMANTIC_TEXCOORD0;
    vertexLayout->attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout->attribs[1].binding = 0;
    vertexLayout->attribs[1].location = 1;
    vertexLayout->attribs[1].offset = TinyImageFormat_BitSizeOfBlock(pUserInterface->vertexLayoutTextured.attribs[0].format) / 8;
    vertexLayout->attribs[2].semantic = SEMANTIC_COLOR;
    vertexLayout->attribs[2].format = TinyImageFormat_R8G8B8A8_UNORM;
    vertexLayout->attribs[2].binding = 0;
    vertexLayout->attribs[2].location = 2;
    vertexLayout->attribs[2].offset =
        vertexLayout->attribs[1].offset + TinyImageFormat_BitSizeOfBlock(pUserInterface->vertexLayoutTextured.attribs[1].format) / 8;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = pDesc->settingsFilename;

    if (pDesc->enableDocking)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Add a default fallback font (at index 0)
    uint32_t fallbackFontTexId = addImguiFont(NULL, 0, NULL, UINT_MAX, 0.f, &pUserInterface->pDefaultFallbackFont);
    ASSERT(fallbackFontTexId == FALLBACK_FONT_TEXTURE_INDEX);

    /************************************************************************/
    // Remote imgui
    /************************************************************************/
#if defined(ENABLE_FORGE_REMOTE_UI)
    if (pUserInterface->enableRemoteUI)
    {
        initRemoteAppServer(8889);
    }
#endif

#endif
}

void exitUserInterface()
{
#ifdef ENABLE_FORGE_UI
    removeSampler(pUserInterface->pRenderer, pUserInterface->pDefaultSampler);

    removeResource(pUserInterface->pVertexBuffer);
    removeResource(pUserInterface->pIndexBuffer);
    for (uint32_t i = 0; i < pUserInterface->frameCount; ++i)
    {
        if (pUserInterface->pUniformBuffer[i])
        {
            removeResource(pUserInterface->pUniformBuffer[i]);
            pUserInterface->pUniformBuffer[i] = NULL;
        }
    }

    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->pCachedFontsArr); ++i)
        removeResource(pUserInterface->pCachedFontsArr[i].pFontTex);

    arrfree(pUserInterface->pCachedFontsArr);
    hmfree(pUserInterface->pTextureHashmap);

    // Resources can no longer be used. Force ImGui to clear all use:
    {
        ImGui::SetCurrentContext(pUserInterface->context);
        uiNewFrame();
        uiEndFrame();
    }

    /************************************************************************/
    // Remote Control
    /************************************************************************/
#if defined(ENABLE_FORGE_REMOTE_UI)
    if (pUserInterface->enableRemoteUI)
    {
        exitRemoteAppServer();
    }
#endif
#endif
}

void loadUserInterface(const UserInterfaceLoadDesc* pDesc)
{
#ifdef ENABLE_FORGE_UI
    if (pDesc->loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        if (pDesc->loadType & RELOAD_TYPE_SHADER)
        {
            const char* imguiFrag[SAMPLE_COUNT_COUNT] = {
                "imgui_SAMPLE_COUNT_1.frag", "imgui_SAMPLE_COUNT_2.frag",  "imgui_SAMPLE_COUNT_4.frag",
                "imgui_SAMPLE_COUNT_8.frag", "imgui_SAMPLE_COUNT_16.frag",
            };
            ShaderLoadDesc texturedShaderDesc = {};
            texturedShaderDesc.stages[0] = { "imgui.vert" };
            for (uint32_t s = 0; s < TF_ARRAY_COUNT(imguiFrag); ++s)
            {
                texturedShaderDesc.stages[1] = { imguiFrag[s] };
                addShader(pUserInterface->pRenderer, &texturedShaderDesc, &pUserInterface->pShaderTextured[s]);
            }

            const char*       pStaticSamplerNames[] = { "uSampler" };
            RootSignatureDesc textureRootDesc = { pUserInterface->pShaderTextured, TF_ARRAY_COUNT(pUserInterface->pShaderTextured) };
            textureRootDesc.staticSamplerCount = 1;
            textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
            textureRootDesc.ppStaticSamplers = &pUserInterface->pDefaultSampler;
            addRootSignature(pUserInterface->pRenderer, &textureRootDesc, &pUserInterface->pRootSignatureTextured);

            DescriptorSetDesc setDesc = { pUserInterface->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
                                          pUserInterface->maxUIFonts +
                                              (pUserInterface->maxDynamicUIUpdatesPerBatch * pUserInterface->frameCount) };
            addDescriptorSet(pUserInterface->pRenderer, &setDesc, &pUserInterface->pDescriptorSetTexture);
            setDesc = { pUserInterface->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_NONE, pUserInterface->frameCount };
            addDescriptorSet(pUserInterface->pRenderer, &setDesc, &pUserInterface->pDescriptorSetUniforms);

            for (uint32_t i = 0; i < pUserInterface->frameCount; ++i)
            {
                DescriptorData params[1] = {};
                params[0].pName = "uniformBlockVS";
                params[0].ppBuffers = &pUserInterface->pUniformBuffer[i];
                updateDescriptorSet(pUserInterface->pRenderer, i, pUserInterface->pDescriptorSetUniforms, 1, params);
            }
        }

        BlendStateDesc blendStateDesc = {};
        blendStateDesc.srcFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.dstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.srcAlphaFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.dstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.colorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateDesc.renderTargetMask = BLEND_STATE_TARGET_ALL;
        blendStateDesc.independentBlend = false;

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.depthTest = false;
        depthStateDesc.depthWrite = false;

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.cullMode = CULL_MODE_NONE;
        rasterizerStateDesc.scissor = true;

        PipelineDesc desc = {};
        desc.pCache = pUserInterface->pPipelineCache;
        desc.type = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineDesc = desc.graphicsDesc;
        pipelineDesc.depthStencilFormat = TinyImageFormat_UNDEFINED;
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.sampleCount = SAMPLE_COUNT_1;
        pipelineDesc.pBlendState = &blendStateDesc;
        pipelineDesc.sampleQuality = 0;
        pipelineDesc.pColorFormats = (TinyImageFormat*)&pDesc->colorFormat;
        pipelineDesc.pDepthState = &depthStateDesc;
        pipelineDesc.pRasterizerState = &rasterizerStateDesc;
        pipelineDesc.pRootSignature = pUserInterface->pRootSignatureTextured;
        pipelineDesc.pVertexLayout = &pUserInterface->vertexLayoutTextured;
        pipelineDesc.primitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineDesc.vrFoveatedRendering = true;
        for (uint32_t s = 0; s < TF_ARRAY_COUNT(pUserInterface->pShaderTextured); ++s)
        {
            pipelineDesc.pShaderProgram = pUserInterface->pShaderTextured[s];
            addPipeline(pUserInterface->pRenderer, &desc, &pUserInterface->pPipelineTextured[s]);
        }
    }

    if (pDesc->loadType & RELOAD_TYPE_RESIZE)
    {
        pUserInterface->width = (float)pDesc->width;
        pUserInterface->height = (float)pDesc->height;
        pUserInterface->displayWidth = pDesc->displayWidth == 0 ? pUserInterface->width : (float)pDesc->displayWidth;
        pUserInterface->displayHeight = pDesc->displayHeight == 0 ? pUserInterface->height : (float)pDesc->displayHeight;
    }

    for (ptrdiff_t tex = 0; tex < arrlen(pUserInterface->pCachedFontsArr); ++tex)
    {
        DescriptorData params[1] = {};
        params[0].pName = "uTex";
        params[0].ppTextures = &pUserInterface->pCachedFontsArr[tex].pFontTex;
        updateDescriptorSet(pUserInterface->pRenderer, (uint32_t)tex, pUserInterface->pDescriptorSetTexture, 1, params);
    }

#if TOUCH_INPUT
    bool loadVirtualJoystick(ReloadType loadType, TinyImageFormat colorFormat, uint32_t width, uint32_t height, uint32_t displayWidth,
                             uint32_t dispayHeight);
    loadVirtualJoystick((ReloadType)pDesc->loadType, (TinyImageFormat)pDesc->colorFormat, pUserInterface->width, pUserInterface->height,
                        pUserInterface->displayWidth, pUserInterface->displayHeight);
#endif
#endif
}

void unloadUserInterface(uint32_t unloadType)
{
#ifdef ENABLE_FORGE_UI
#if TOUCH_INPUT
    extern void unloadVirtualJoystick(ReloadType unloadType);
    unloadVirtualJoystick((ReloadType)unloadType);
#endif

    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        for (uint32_t s = 0; s < TF_ARRAY_COUNT(pUserInterface->pShaderTextured); ++s)
        {
            removePipeline(pUserInterface->pRenderer, pUserInterface->pPipelineTextured[s]);
        }

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            for (uint32_t s = 0; s < TF_ARRAY_COUNT(pUserInterface->pShaderTextured); ++s)
            {
                removeShader(pUserInterface->pRenderer, pUserInterface->pShaderTextured[s]);
            }
            removeDescriptorSet(pUserInterface->pRenderer, pUserInterface->pDescriptorSetTexture);
            removeDescriptorSet(pUserInterface->pRenderer, pUserInterface->pDescriptorSetUniforms);
            removeRootSignature(pUserInterface->pRenderer, pUserInterface->pRootSignatureTextured);
        }
    }
#endif
}

void cmdDrawUserInterface(Cmd* pCmd, UserInterfaceDrawData* pUIDrawData)
{
#ifdef ENABLE_FORGE_UI

#if defined(ENABLE_FORGE_REMOTE_UI)
    bool removeDataAfterRendering = false;
#else
    // Early return if UI rendering has been disabled
    if (!pUserInterface->enableRendering)
    {
        return;
    }
#endif

    UserInterfaceDrawData localDrawData = {};

    if (!pUIDrawData)
    {
#if defined(ENABLE_FORGE_REMOTE_UI)
        if (pUserInterface->enableRemoteUI && remoteAppIsConnected())
        {
            UserInterfaceDrawData remoteDrawData = {};
            uiPopulateDrawData(&remoteDrawData);
            remoteAppSendDrawData(&remoteDrawData);
            pUIDrawData = &remoteDrawData;
            removeDataAfterRendering = true;
        }
        else
        {
            ImGui::SetCurrentContext(pUserInterface->context);
            ImGui::Render();
            ImDrawData* pImDrawData = ImGui::GetDrawData();
            fillDrawInformation(&localDrawData, pImDrawData);
            pUIDrawData = &localDrawData;
        }

        if (!pUserInterface->enableRendering)
        {
            if (removeDataAfterRendering)
            {
                removeUIDrawData(pUIDrawData);
            }
            return;
        }
#else
        ImGui::SetCurrentContext(pUserInterface->context);
        ImGui::Render();
        ImDrawData* pImDrawData = ImGui::GetDrawData();
        fillDrawInformation(&localDrawData, pImDrawData);
        pUIDrawData = &localDrawData;
#endif
    }

    float2  displayPos(0.f, 0.f);
    float2  displaySize(0.f, 0.f);
    int32_t numDrawCommands = 0;

    displayPos = pUIDrawData->displayPos;
    displaySize = pUIDrawData->displaySize;
    numDrawCommands = pUIDrawData->numDrawCommands;

    uint64_t vSize = pUIDrawData->vertexCount * pUIDrawData->vertexSize;
    uint64_t iSize = pUIDrawData->indexCount * pUIDrawData->indexSize;
    vSize = min<uint64_t>(vSize, VERTEX_BUFFER_SIZE);
    iSize = min<uint64_t>(iSize, INDEX_BUFFER_SIZE);

    uint64_t vOffset = pUserInterface->frameIdx * VERTEX_BUFFER_SIZE;
    uint64_t iOffset = pUserInterface->frameIdx * INDEX_BUFFER_SIZE;

    if (pUIDrawData->vertexCount > FORGE_UI_MAX_VERTEXES || pUIDrawData->indexCount > FORGE_UI_MAX_INDEXES)
    {
        LOGF(eWARNING, "UI exceeds amount of verts/inds.  Consider updating FORGE_UI_MAX_VERTEXES/FORGE_UI_MAX_INDEXES defines.");
        LOGF(eWARNING, "Num verts: %u (max %d) | Num inds: %u (max %d)", pUIDrawData->vertexCount, FORGE_UI_MAX_VERTEXES,
             pUIDrawData->indexCount, FORGE_UI_MAX_INDEXES);
        pUIDrawData->vertexCount = pUIDrawData->vertexCount > FORGE_UI_MAX_VERTEXES ? FORGE_UI_MAX_VERTEXES : pUIDrawData->vertexCount;
        pUIDrawData->indexCount = pUIDrawData->indexCount > FORGE_UI_MAX_INDEXES ? FORGE_UI_MAX_INDEXES : pUIDrawData->indexCount;
    }

    uint64_t vtxDst = vOffset;
    uint64_t idxDst = iOffset;

    if (pUIDrawData->vertexBufferData && pUIDrawData->indexBufferData)
    {
        BufferUpdateDesc update = { pUserInterface->pVertexBuffer, vOffset };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, pUIDrawData->vertexBufferData, (size_t)pUIDrawData->vertexCount * pUIDrawData->vertexSize);
        endUpdateResource(&update);

        update = { pUserInterface->pIndexBuffer, iOffset };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, pUIDrawData->indexBufferData, (size_t)pUIDrawData->indexCount * pUIDrawData->indexSize);
        endUpdateResource(&update);
    }
    else
    {
        ImDrawData* pImDrawData = ImGui::GetDrawData();

        for (int32_t i = 0; i < pImDrawData->CmdListsCount; i++)
        {
            const ImDrawList* pCmdList = pImDrawData->CmdLists[i];
            BufferUpdateDesc  update = { pUserInterface->pVertexBuffer, vtxDst };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.size() * sizeof(ImDrawVert));
            endUpdateResource(&update);

            update = { pUserInterface->pIndexBuffer, idxDst };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
            endUpdateResource(&update);

            vtxDst += (pCmdList->VtxBuffer.size() * sizeof(ImDrawVert));
            idxDst += (pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
        }
    }

    Pipeline* pPipeline = pUserInterface->pPipelineTextured[0];
    Pipeline* pPreviousPipeline = pPipeline;
    uint32_t  prevSetIndex = UINT32_MAX;

    cmdPrepareRenderingForUI(pCmd, displayPos, displaySize, pPipeline, vOffset, iOffset);

    // Render command lists
    int32_t globalVtxOffset = 0;
    int32_t globalIdxOffset = 0;

    if (pUIDrawData->drawCommands)
    {
        for (int32_t i = 0; i < numDrawCommands; i++)
        {
            cmdDrawUICommand(pCmd, &pUIDrawData->drawCommands[i], displayPos, displaySize, &pPipeline, &pPreviousPipeline, globalVtxOffset,
                             globalIdxOffset, prevSetIndex);
        }
    }
    else
    {
        ImDrawData* pImDrawData = ImGui::GetDrawData();
        for (int n = 0; n < pImDrawData->CmdListsCount; n++)
        {
            const ImDrawList* pCmdList = pImDrawData->CmdLists[n];

            for (int c = 0; c < pCmdList->CmdBuffer.size(); c++)
            {
                const ImDrawCmd* pImDrawCmd = &pCmdList->CmdBuffer[c];

                if (pImDrawCmd->UserCallback)
                {
                    // User callback (registered via ImDrawList::AddCallback)
                    pImDrawCmd->UserCallback(pCmdList, pImDrawCmd);
                    continue;
                }

                UserInterfaceDrawCommand drawCommand = {};
                drawCommand.clipRect = pImDrawCmd->ClipRect;
                drawCommand.textureId = (uint64_t)pImDrawCmd->TextureId;
                drawCommand.vertexOffset = pImDrawCmd->VtxOffset;
                drawCommand.indexOffset = pImDrawCmd->IdxOffset;
                if (c == pCmdList->CmdBuffer.size() - 1)
                {
                    drawCommand.vertexCount = pCmdList->VtxBuffer.size();
                    drawCommand.indexCount = pCmdList->IdxBuffer.size();
                }
                else
                {
                    drawCommand.vertexCount = 0;
                    drawCommand.indexCount = 0;
                }
                drawCommand.elemCount = pImDrawCmd->ElemCount;
                cmdDrawUICommand(pCmd, &drawCommand, displayPos, displaySize, &pPipeline, &pPreviousPipeline, globalVtxOffset,
                                 globalIdxOffset, prevSetIndex);
            }
        }
    }

    pUserInterface->frameIdx = (pUserInterface->frameIdx + 1) % pUserInterface->frameCount;

#if TOUCH_INPUT
    extern void drawVirtualJoystick(Cmd * pCmd, const float4* color);
    float4      color{ 1.0f, 1.0f, 1.0f, 1.0f };
    drawVirtualJoystick(pCmd, &color);
#endif

#if defined(ENABLE_FORGE_REMOTE_UI)
    if (removeDataAfterRendering)
    {
        removeUIDrawData(pUIDrawData);
    }
#endif
#endif
}

void uiOnText(const wchar_t* pText)
{
#ifdef ENABLE_FORGE_UI
    ImGui::SetCurrentContext(pUserInterface->context);
    ImGuiIO& io = ImGui::GetIO();
    uint32_t len = (uint32_t)wcslen(pText);
    for (uint32_t i = 0; i < len; ++i)
        io.AddInputCharacter(pText[i]);
#endif
}

void uiOnButton(uint32_t actionId, bool press, const float2* pVec)
{
#ifdef ENABLE_FORGE_UI
    ImGui::SetCurrentContext(pUserInterface->context);
    ImGuiIO& io = ImGui::GetIO();
    if (pVec)
        pUserInterface->pMovePosition = pVec;

    switch (actionId)
    {
    case UISystemInputActions::UI_ACTION_NAV_TOGGLE_UI:
        if (!press)
        {
            pUserInterface->active = !pUserInterface->active;
        }
        break;
    case UISystemInputActions::UI_ACTION_NAV_HIDE_UI_TOGGLE:
        if (!press)
        {
            pUserInterface->enableRendering = !pUserInterface->enableRendering;
        }
        break;
    case UISystemInputActions::UI_ACTION_NAV_ACTIVATE:
        pUserInterface->navInputs[ImGuiNavInput_Activate] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_CANCEL:
        pUserInterface->navInputs[ImGuiNavInput_Cancel] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_INPUT:
        pUserInterface->navInputs[ImGuiNavInput_Input] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_MENU:
        pUserInterface->navInputs[ImGuiNavInput_Menu] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_LEFT:
        pUserInterface->navInputs[ImGuiNavInput_DpadLeft] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_RIGHT:
        pUserInterface->navInputs[ImGuiNavInput_DpadRight] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_UP:
        pUserInterface->navInputs[ImGuiNavInput_DpadUp] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_TWEAK_WINDOW_DOWN:
        pUserInterface->navInputs[ImGuiNavInput_DpadDown] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_FOCUS_PREV:
        pUserInterface->navInputs[ImGuiNavInput_FocusPrev] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_FOCUS_NEXT:
        pUserInterface->navInputs[ImGuiNavInput_FocusNext] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_TWEAK_SLOW:
        pUserInterface->navInputs[ImGuiNavInput_TweakSlow] = (float)press;
        break;
    case UISystemInputActions::UI_ACTION_NAV_TWEAK_FAST:
        pUserInterface->navInputs[ImGuiNavInput_TweakFast] = (float)press;
        break;

    case UISystemInputActions::UI_ACTION_KEY_CONTROL_L:
    case UISystemInputActions::UI_ACTION_KEY_CONTROL_R:
        io.KeyCtrl = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_SHIFT_L:
    case UISystemInputActions::UI_ACTION_KEY_SHIFT_R:
        io.KeyShift = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_ALT_L:
    case UISystemInputActions::UI_ACTION_KEY_ALT_R:
        io.KeyAlt = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_SUPER_L:
    case UISystemInputActions::UI_ACTION_KEY_SUPER_R:
        io.KeySuper = press;
        break;
    case UISystemInputActions::UI_ACTION_MOUSE_LEFT:
    case UISystemInputActions::UI_ACTION_MOUSE_RIGHT:
    case UISystemInputActions::UI_ACTION_MOUSE_MIDDLE:
    case UISystemInputActions::UI_ACTION_MOUSE_SCROLL_UP:
    case UISystemInputActions::UI_ACTION_MOUSE_SCROLL_DOWN:
    case UISystemInputActions::UI_ACTION_MOUSE_MOVE:
    {
        const float scrollScale =
            0.25f; // This should maybe be customized by client?  1.f would scroll ~5 lines of txt according to ImGui doc.
        pUserInterface->navInputs[ImGuiNavInput_Activate] = (float)press;
        if (pVec)
        {
            if (UISystemInputActions::UI_ACTION_MOUSE_LEFT == actionId)
                io.MouseDown[0] = press;
            else if (UISystemInputActions::UI_ACTION_MOUSE_RIGHT == actionId)
                io.MouseDown[1] = press;
            else if (UISystemInputActions::UI_ACTION_MOUSE_MIDDLE == actionId)
                io.MouseDown[2] = press;
            else if (UISystemInputActions::UI_ACTION_MOUSE_SCROLL_UP == actionId)
                io.MouseWheel = 1.f * scrollScale;
            else if (UISystemInputActions::UI_ACTION_MOUSE_SCROLL_DOWN == actionId)
                io.MouseWheel = -1.f * scrollScale;
        }
        if (io.MousePos.x != -FLT_MAX && io.MousePos.y != -FLT_MAX)
        {
            io.MousePos = *pVec;
            break;
        }
        else if (pVec)
        {
            io.MousePos = *pVec;
            for (uint32_t i = 0; i < pUserInterface->lastUpdateCount; ++i)
            {
                if (ImGui::IsMouseHoveringRect(pUserInterface->lastUpdateMin[i], pUserInterface->lastUpdateMax[i], false))
                {
                    // TOOD: io.WantCaptureMouse is meant to be for the application to read, ImGui modifies it internally. We should find
                    // another way to do this rather than changing it.
                    io.WantCaptureMouse = true;
                }
            }
            break;
        }
        break;
    }

    // Note that for keyboard keys, we only set them to true here if they are pressed because we may have a press/release
    // happening in one frame and it would never get registered.  Instead, unpressed are deferred at the end of update().
    // This scenario occurs with mobile soft (on-screen) keyboards.
    case UISystemInputActions::UI_ACTION_KEY_TAB:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_TAB] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_TAB] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_LEFT_ARROW:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_LEFT_ARROW] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_LEFT_ARROW] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_RIGHT_ARROW:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_RIGHT_ARROW] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_RIGHT_ARROW] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_UP_ARROW:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_UP_ARROW] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_UP_ARROW] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_DOWN_ARROW:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_DOWN_ARROW] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_DOWN_ARROW] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_PAGE_UP:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_PAGE_UP] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_PAGE_UP] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_PAGE_DOWN:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_PAGE_DOWN] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_PAGE_DOWN] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_HOME:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_HOME] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_HOME] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_END:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_END] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_END] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_INSERT:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_INSERT] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_INSERT] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_DELETE:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_DELETE] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_DELETE] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_BACK_SPACE:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_BACK_SPACE] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_BACK_SPACE] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_SPACE:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_SPACE] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_SPACE] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_ENTER:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_ENTER] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_ENTER] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_ESCAPE:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_ESCAPE] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_ESCAPE] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_A:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_A] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_A] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_C:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_C] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_C] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_V:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_V] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_V] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_X:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_X] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_X] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_Y:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_Y] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_Y] = press;
        break;
    case UISystemInputActions::UI_ACTION_KEY_Z:
        if (press)
            io.KeysDown[UISystemInputActions::UI_ACTION_KEY_Z] = true;
        pUserInterface->postUpdateKeyDownStates[UISystemInputActions::UI_ACTION_KEY_Z] = press;
        break;

    default:
        break;
    }
#endif
}

void uiOnStick(uint32_t actionId, const float2* pStick)
{
#ifdef ENABLE_FORGE_UI
    ImGui::SetCurrentContext(pUserInterface->context);

    switch (actionId)
    {
    case UISystemInputActions::UI_ACTION_NAV_SCROLL_MOVE_WINDOW:
    {
        ASSERT(pStick);
        const float2 vec = *pStick;
        if (vec.x < 0.f)
            pUserInterface->navInputs[ImGuiNavInput_LStickLeft] = abs(vec.x);
        else if (vec.x > 0.f)
            pUserInterface->navInputs[ImGuiNavInput_LStickRight] = vec.x;
        else
        {
            pUserInterface->navInputs[ImGuiNavInput_LStickLeft] = 0.f;
            pUserInterface->navInputs[ImGuiNavInput_LStickRight] = 0.f;
        }

        if (vec.y < 0.f)
            pUserInterface->navInputs[ImGuiNavInput_LStickDown] = abs(vec.y);
        else if (vec.y > 0.f)
            pUserInterface->navInputs[ImGuiNavInput_LStickUp] = vec.y;
        else
        {
            pUserInterface->navInputs[ImGuiNavInput_LStickDown] = 0.f;
            pUserInterface->navInputs[ImGuiNavInput_LStickUp] = 0.f;
        }

        break;
    }
    }
#endif
}

void uiOnInput(uint32_t actionId, bool buttonPress, const float2* pMousePos, const float2* pStick)
{
#ifdef ENABLE_FORGE_UI
    if (actionId == UISystemInputActions::UI_ACTION_NAV_SCROLL_MOVE_WINDOW)
        uiOnStick(actionId, pStick);
    else
        uiOnButton(actionId, buttonPress, pMousePos);
#endif
}

uint8_t uiWantTextInput()
{
#ifdef ENABLE_FORGE_UI
    ImGui::SetCurrentContext(pUserInterface->context);
    // The User flags are not what I expect them to be.
    // We need access to Per-Component InputFlags
    ImGuiContext*       guiContext = (ImGuiContext*)pUserInterface->context;
    ImGuiInputTextFlags currentInputFlags = guiContext->InputTextState.Flags;

    // 0 -> Not pressed
    // 1 -> Digits Only keyboard
    // 2 -> Full Keyboard (Chars + Digits)
    int inputState = ImGui::GetIO().WantTextInput ? 2 : 0;
    // keyboard only Numbers
    if (inputState > 0 && (currentInputFlags & ImGuiInputTextFlags_CharsDecimal))
    {
        inputState = 1;
    }

    return (uint8_t)inputState;
#else
    return 0;
#endif
}

bool uiIsFocused()
{
#ifdef ENABLE_FORGE_UI
    if (!pUserInterface->active)
    {
        return false;
    }
    ImGui::SetCurrentContext(pUserInterface->context);
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard || io.NavVisible;
#else
    return false;
#endif
}

void uiToggleRendering(bool enabled)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUserInterface);
    pUserInterface->enableRendering = enabled;
#endif
}

bool uiIsRenderingEnabled()
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUserInterface);
    return pUserInterface->enableRendering;
#else
    return false;
#endif
}

UserInterfaceDrawData* addUIDrawData()
{
    UserInterfaceDrawData* pRet = (UserInterfaceDrawData*)tf_calloc(1, sizeof(UserInterfaceDrawData));
    return pRet;
}

void uiPopulateDrawData(UserInterfaceDrawData* pUIDrawData)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUIDrawData);
    ASSERT(pUserInterface);

    ImGui::SetCurrentContext(pUserInterface->context);
    ImGui::Render();

    ImDrawData* pImDrawData = ImGui::GetDrawData();
    fillDrawInformation(pUIDrawData, pImDrawData);

    pUIDrawData->vertexBufferData = (unsigned char*)tf_malloc(pUIDrawData->vertexCount * sizeof(ImDrawVert));
    pUIDrawData->indexBufferData = (unsigned char*)tf_malloc(pUIDrawData->indexCount * sizeof(ImDrawIdx));

    uint64_t vtxOffset = 0;
    uint64_t idxOffset = 0;
    for (int n = 0; n < pImDrawData->CmdListsCount; n++)
    {
        const ImDrawList* pCmdList = pImDrawData->CmdLists[n];
        memcpy(pUIDrawData->vertexBufferData + vtxOffset, pCmdList->VtxBuffer.Data,
               (size_t)pCmdList->VtxBuffer.size() * sizeof(ImDrawVert));
        memcpy(pUIDrawData->indexBufferData + idxOffset, pCmdList->IdxBuffer.Data, (size_t)pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx));

        vtxOffset += (pCmdList->VtxBuffer.size() * sizeof(ImDrawVert));
        idxOffset += (pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
    }

    if (!pUIDrawData->drawCommands || arrlen(pUIDrawData->drawCommands) < pUIDrawData->numDrawCommands)
    {
        arrsetlen(pUIDrawData->drawCommands, 0);
        arrsetlen(pUIDrawData->drawCommands, pUIDrawData->numDrawCommands);
    }

    uint32_t drawCommandsOffset = 0;
    for (int n = 0; n < pImDrawData->CmdListsCount; n++)
    {
        const ImDrawList* pCmdList = pImDrawData->CmdLists[n];

        for (int c = 0; c < pCmdList->CmdBuffer.size(); c++)
        {
            const ImDrawCmd* pImDrawCmd = &pCmdList->CmdBuffer[c];

            if (pUIDrawData->drawCommands)
            {
                pUIDrawData->drawCommands[drawCommandsOffset].clipRect = pImDrawCmd->ClipRect;
                pUIDrawData->drawCommands[drawCommandsOffset].textureId = (uint64_t)pImDrawCmd->TextureId;
                pUIDrawData->drawCommands[drawCommandsOffset].vertexOffset = pImDrawCmd->VtxOffset;
                pUIDrawData->drawCommands[drawCommandsOffset].indexOffset = pImDrawCmd->IdxOffset;
                if (c == pCmdList->CmdBuffer.size() - 1)
                {
                    pUIDrawData->drawCommands[drawCommandsOffset].vertexCount = pCmdList->VtxBuffer.size();
                    pUIDrawData->drawCommands[drawCommandsOffset].indexCount = pCmdList->IdxBuffer.size();
                }
                else
                {
                    pUIDrawData->drawCommands[drawCommandsOffset].vertexCount = 0;
                    pUIDrawData->drawCommands[drawCommandsOffset].indexCount = 0;
                }
                pUIDrawData->drawCommands[drawCommandsOffset].elemCount = pImDrawCmd->ElemCount;

                drawCommandsOffset++;
            }
        }
    }
#endif
}

void removeUIDrawData(UserInterfaceDrawData* pUIDrawData)
{
    if (pUIDrawData)
    {
        tf_free(pUIDrawData->vertexBufferData);
        tf_free(pUIDrawData->indexBufferData);
        arrfree(pUIDrawData->drawCommands);
    }
}
