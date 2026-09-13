#ifndef DGA_INPUT
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

#include <ThirdParty/stb/stb_ds.h>

#include "RHI/IGraphics.h"

#ifdef ENABLE_FORGE_INPUT
#include <ThirdParty/gainput/lib/include/gainput/gainput.h>
#endif

#if defined(__ANDROID__) || defined(NX64)
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputInputDeltaState.h"
#endif

#ifdef __APPLE__
#ifdef TARGET_IOS
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/apple/GainputIos.h"
#else
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/apple/GainputMac.h"
#endif
#endif

#ifdef __linux__
#include <climits>
#endif

#ifdef METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#ifdef TARGET_IOS
#include <UIKit/UIView.h>

#include "../Application/ThirdParty/OpenSource/gainput/lib/source/gainput/apple/GainputInputDeviceTouchIos.h"
#else
#import <Cocoa/Cocoa.h>
#endif
#endif

#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>

#include "RHI/IGraphics.h"
#include "Platform/IOperatingSystem.h"
#include "Resources/IResourceLoader.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "Platform/IInput.h"
#include "Application/IUI.h"

#include "Core/IMemory.h"

#ifdef GAINPUT_PLATFORM_GGP
namespace gainput
{
extern void SetWindow(void* pData);
}
#endif

#define MAX_DEVICES 16U

#if (defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)) && !defined(QUEST_VR)
#define TOUCH_INPUT 1
#endif

#if TOUCH_INPUT
#define TOUCH_DOWN(id)     (((id) << 2) + 0)
#define TOUCH_X(id)        (((id) << 2) + 1)
#define TOUCH_Y(id)        (((id) << 2) + 2)
#define TOUCH_PRESSURE(id) (((id) << 2) + 3)
#define TOUCH_USER(btn)    ((btn) >> 2)
#define TOUCH_AXIS(btn)    (((btn) % 4) - 1)

// gainput::TouchButton enum has four values for each finger touch, for finger 0 these are: Touch0Down, Touch0X, Touch0Y, Touch0Pressure
// by dividing by four we get the finger id
FORGE_CONSTEXPR const uint32_t GAINPUT_TOUCH_BUTTONS_PER_FINGER = 4;
#endif

/**********************************************/
// VirtualJoystick
/**********************************************/

typedef struct VirtualJoystickDesc
{
    Renderer*   pRenderer;
    const char* pJoystickTexture;

} VirtualJoystickDesc;

typedef struct VirtualJoystick
{
#if TOUCH_INPUT
    Renderer*      pRenderer = NULL;
    Shader*        pShader = NULL;
    RootSignature* pRootSignature = NULL;
    DescriptorSet* pDescriptorSet = NULL;
    Pipeline*      pPipeline = NULL;
    Texture*       pTexture = NULL;
    Sampler*       pSampler = NULL;
    Buffer*        pMeshBuffer = NULL;
    float2         renderSize = float2(0.f, 0.f);
    float2         renderScale = float2(0.f, 0.f);

    // input related
    float    insideRadius = 100.f;
    float    outsideRadius = 200.f;
    uint32_t rootConstantIndex;

    struct StickInput
    {
        bool   pressed = false;
        float2 startPos = float2(0.f, 0.f);
        float2 currPos = float2(0.f, 0.f);
    };
    // Left -> Index 0
    // Right -> Index 1
    StickInput sticks[2];
#endif
} VirtualJoystick;

static VirtualJoystick* gVirtualJoystick = NULL;

void initVirtualJoystick(VirtualJoystickDesc* pDesc, VirtualJoystick** ppVirtualJoystick)
{
    UNREF_PARAM(pDesc);
    ASSERT(ppVirtualJoystick);
    ASSERT(gVirtualJoystick == NULL);

    gVirtualJoystick = tf_new(VirtualJoystick);

#if TOUCH_INPUT
    Renderer* pRenderer = (Renderer*)pDesc->pRenderer;
    gVirtualJoystick->pRenderer = pRenderer;

    TextureLoadDesc loadDesc = {};
    SyncToken       token = {};
    loadDesc.pFileName = pDesc->pJoystickTexture;
    loadDesc.ppTexture = &gVirtualJoystick->pTexture;
    // Textures representing color should be stored in SRGB or HDR format
    loadDesc.creationFlag = TEXTURE_CREATION_FLAG_SRGB;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    if (!gVirtualJoystick->pTexture)
    {
        LOGF(LogLevel::eWARNING, "Could not load virtual joystick texture file: %s", pDesc->pJoystickTexture);
        tf_delete(gVirtualJoystick);
        gVirtualJoystick = NULL;
        return;
    }
    /************************************************************************/
    // States
    /************************************************************************/
    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(pRenderer, &samplerDesc, &gVirtualJoystick->pSampler);
    /************************************************************************/
    // Resources
    /************************************************************************/
    BufferLoadDesc vbDesc = {};
    vbDesc.desc.descriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.desc.memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.desc.flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.desc.size = 128 * 4 * sizeof(float4);
    vbDesc.ppBuffer = &gVirtualJoystick->pMeshBuffer;
    addResource(&vbDesc, NULL);
#endif

    // Joystick is good!
    *ppVirtualJoystick = gVirtualJoystick;
}

void exitVirtualJoystick(VirtualJoystick** ppVirtualJoystick)
{
    ASSERT(ppVirtualJoystick);
    VirtualJoystick* pVirtualJoystick = *ppVirtualJoystick;
    if (!pVirtualJoystick)
        return;

#if TOUCH_INPUT
    removeSampler(pVirtualJoystick->pRenderer, pVirtualJoystick->pSampler);
    removeResource(pVirtualJoystick->pMeshBuffer);
    removeResource(pVirtualJoystick->pTexture);
#endif

    tf_delete(pVirtualJoystick);
    *ppVirtualJoystick = NULL;
}

bool loadVirtualJoystick(ReloadType loadType, hz::Format colorFormat, uint32_t width, uint32_t height, uint32_t displayWidth,
                         uint32_t displayHeight)
{
    UNREF_PARAM(loadType);
    UNREF_PARAM(colorFormat);
    UNREF_PARAM(width);
    UNREF_PARAM(height);
    UNREF_PARAM(displayWidth);
    UNREF_PARAM(displayHeight);
#if TOUCH_INPUT
    if (!gVirtualJoystick)
    {
        return false;
    }

    if (loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        Renderer* pRenderer = gVirtualJoystick->pRenderer;

        if (loadType & RELOAD_TYPE_SHADER)
        {
            /************************************************************************/
            // Shader
            /************************************************************************/
            ShaderLoadDesc texturedShaderDesc = {};
            texturedShaderDesc.stages[0].pFileName = "textured_mesh.vert";
            texturedShaderDesc.stages[1].pFileName = "textured_mesh.frag";
            addShader(pRenderer, &texturedShaderDesc, &gVirtualJoystick->pShader);

            const char*       pStaticSamplerNames[] = { "uSampler" };
            RootSignatureDesc textureRootDesc = { &gVirtualJoystick->pShader, 1 };
            textureRootDesc.staticSamplerCount = 1;
            textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
            textureRootDesc.ppStaticSamplers = &gVirtualJoystick->pSampler;
            addRootSignature(pRenderer, &textureRootDesc, &gVirtualJoystick->pRootSignature);
            gVirtualJoystick->rootConstantIndex = getDescriptorIndexFromName(gVirtualJoystick->pRootSignature, "uRootConstants");

            DescriptorSetDesc descriptorSetDesc = { gVirtualJoystick->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(pRenderer, &descriptorSetDesc, &gVirtualJoystick->pDescriptorSet);
            /************************************************************************/
            // Prepare descriptor sets
            /************************************************************************/
            DescriptorData params[1] = {};
            params[0].pName = "uTex";
            params[0].ppTextures = &gVirtualJoystick->pTexture;
            updateDescriptorSet(pRenderer, 0, gVirtualJoystick->pDescriptorSet, 1, params);
        }

        VertexLayout vertexLayout = {};
        vertexLayout.bindingCount = 1;
        vertexLayout.attribCount = 2;
        vertexLayout.attribs[0].semantic = SEMANTIC_POSITION;
        vertexLayout.attribs[0].format = hz::Format::R32G32_SFLOAT;
        vertexLayout.attribs[0].binding = 0;
        vertexLayout.attribs[0].location = 0;
        vertexLayout.attribs[0].offset = 0;

        vertexLayout.attribs[1].semantic = SEMANTIC_TEXCOORD0;
        vertexLayout.attribs[1].format = hz::Format::R32G32_SFLOAT;
        vertexLayout.attribs[1].binding = 0;
        vertexLayout.attribs[1].location = 1;
        vertexLayout.attribs[1].offset = TinyImageFormat_BitSizeOfBlock((TinyImageFormat)hz::Format::R32G32_SFLOAT) / 8;

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
        desc.type = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineDesc = desc.graphicsDesc;
        pipelineDesc.primitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
        pipelineDesc.depthStencilFormat = hz::Format::UNDEFINED;
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.sampleCount = SAMPLE_COUNT_1;
        pipelineDesc.sampleQuality = 0;
        pipelineDesc.pBlendState = &blendStateDesc;
        pipelineDesc.pColorFormats = &colorFormat;
        pipelineDesc.pDepthState = &depthStateDesc;
        pipelineDesc.pRasterizerState = &rasterizerStateDesc;
        pipelineDesc.pRootSignature = gVirtualJoystick->pRootSignature;
        pipelineDesc.pShaderProgram = gVirtualJoystick->pShader;
        pipelineDesc.pVertexLayout = &vertexLayout;
        addPipeline(gVirtualJoystick->pRenderer, &desc, &gVirtualJoystick->pPipeline);
    }

    if (loadType & RELOAD_TYPE_RESIZE)
    {
        gVirtualJoystick->renderSize[0] = (float)width;
        gVirtualJoystick->renderSize[1] = (float)height;
        gVirtualJoystick->renderScale[0] = (float)width / (float)displayWidth;
        gVirtualJoystick->renderScale[1] = (float)height / (float)displayHeight;
    }
#endif
    return true;
}

void unloadVirtualJoystick(ReloadType unloadType)
{
    UNREF_PARAM(unloadType);
#if TOUCH_INPUT
    if (!gVirtualJoystick)
    {
        return;
    }

    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        Renderer* pRenderer = gVirtualJoystick->pRenderer;

        removePipeline(pRenderer, gVirtualJoystick->pPipeline);

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSet(pRenderer, gVirtualJoystick->pDescriptorSet);
            removeRootSignature(pRenderer, gVirtualJoystick->pRootSignature);
            removeShader(pRenderer, gVirtualJoystick->pShader);
        }
    }
#endif
}

void drawVirtualJoystick(Cmd* pCmd, const float4* color)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(color);
#if TOUCH_INPUT
    if (!gVirtualJoystick || !(gVirtualJoystick->sticks[0].pressed || gVirtualJoystick->sticks[1].pressed))
        return;

    struct RootConstants
    {
        float4 color;
        float2 scaleBias;
        int    _pad[2];
    } data = {};

    cmdSetViewport(pCmd, 0.0f, 0.0f, gVirtualJoystick->renderSize[0], gVirtualJoystick->renderSize[1], 0.0f, 1.0f);
    cmdSetScissor(pCmd, 0u, 0u, (uint32_t)gVirtualJoystick->renderSize[0], (uint32_t)gVirtualJoystick->renderSize[1]);

    cmdBindPipeline(pCmd, gVirtualJoystick->pPipeline);
    cmdBindDescriptorSet(pCmd, 0, gVirtualJoystick->pDescriptorSet);
    data.color = *color;
    data.scaleBias = { 2.0f / (float)gVirtualJoystick->renderSize[0], -2.0f / (float)gVirtualJoystick->renderSize[1] };
    cmdBindPushConstants(pCmd, gVirtualJoystick->pRootSignature, gVirtualJoystick->rootConstantIndex, &data);

    // Draw the camera controller's virtual joysticks.
    float extSide = gVirtualJoystick->outsideRadius;
    float intSide = gVirtualJoystick->insideRadius;

    uint64_t bufferOffset = 0;
    for (uint i = 0; i < 2; i++)
    {
        if (gVirtualJoystick->sticks[i].pressed)
        {
            float2 joystickSize = float2(extSide) * gVirtualJoystick->renderScale;
            float2 joystickCenter =
                gVirtualJoystick->sticks[i].startPos * gVirtualJoystick->renderScale - float2(0.0f, gVirtualJoystick->renderSize.y * 0.1f);
            float2 joystickPos = joystickCenter - joystickSize * 0.5f;

            const uint32_t   vertexStride = sizeof(float4);
            BufferUpdateDesc updateDesc = { gVirtualJoystick->pMeshBuffer, bufferOffset };
            beginUpdateResource(&updateDesc);
            TexVertex vertices[4] = {};
            // the last variable can be used to create a border
            MAKETEXQUAD(vertices, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
            memcpy(updateDesc.pMappedData, vertices, sizeof(vertices));
            endUpdateResource(&updateDesc);
            cmdBindVertexBuffer(pCmd, 1, &gVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
            cmdDraw(pCmd, 4, 0);
            bufferOffset += sizeof(TexVertex) * 4;

            joystickSize = float2(intSide) * gVirtualJoystick->renderScale;
            joystickCenter =
                gVirtualJoystick->sticks[i].currPos * gVirtualJoystick->renderScale - float2(0.0f, gVirtualJoystick->renderSize.y * 0.1f);
            joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

            updateDesc = { gVirtualJoystick->pMeshBuffer, bufferOffset };
            beginUpdateResource(&updateDesc);
            TexVertex verticesInner[4] = {};
            // the last variable can be used to create a border
            MAKETEXQUAD(verticesInner, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
            memcpy(updateDesc.pMappedData, verticesInner, sizeof(verticesInner));
            endUpdateResource(&updateDesc);
            cmdBindVertexBuffer(pCmd, 1, &gVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
            cmdDraw(pCmd, 4, 0);
            bufferOffset += sizeof(TexVertex) * 4;
        }
    }
#endif
}

uint8_t virtualJoystickIndexFromArea(TouchScreenArea area)
{
    switch (area)
    {
    case AREA_LEFT:
        return 0;
    case AREA_RIGHT:
        return 1;
    case AREA_FULL:
    default:
        ASSERT(0 && "VirtualJoystick expects AREA_LEFT or AREA_RIGHT");
        break;
    }

    return 0;
}

bool isPositionInsideScreenArea(float2 position, TouchScreenArea area, float2 displaySize)
{
    if (area == AREA_FULL)
        return true;
    else if (area == AREA_LEFT)
        return position.x <= displaySize.x * 0.5f;
    else if (area == AREA_RIGHT)
        return position.x > displaySize.x * 0.5f;

    return false;
}

void virtualJoystickOnMove(VirtualJoystick* pVirtualJoystick, uint32_t id, InputActionContext* ctx)
{
    UNREF_PARAM(pVirtualJoystick);
    UNREF_PARAM(id);
    UNREF_PARAM(ctx);
#if TOUCH_INPUT
    if (!ctx->pPosition)
        return;

    if (*ctx->pCaptured
#ifdef ENABLE_FORGE_UI
        && !uiIsFocused()
#endif
    )
    {
        if (!pVirtualJoystick->sticks[id].pressed)
        {
            pVirtualJoystick->sticks[id].startPos = *ctx->pPosition;
            pVirtualJoystick->sticks[id].currPos = *ctx->pPosition;
        }
        else
        {
            pVirtualJoystick->sticks[id].currPos = *ctx->pPosition;
        }
        pVirtualJoystick->sticks[id].pressed = ctx->phase != INPUT_ACTION_PHASE_CANCELED;
    }
#endif
}
#endif

/**********************************************/
// InputSystem
/**********************************************/
#ifdef ENABLE_FORGE_INPUT
struct InputSystemImpl: public gainput::InputListener
{
    // **********************************************
    // ***** Structures

    enum InputControlType
    {
        CONTROL_BUTTON = 0,
        CONTROL_FLOAT,
        CONTROL_AXIS,
        CONTROL_VIRTUAL_JOYSTICK,
        CONTROL_COMPOSITE,
        CONTROL_COMBO,
        CONTROL_GESTURE,
    };

    struct IControl
    {
        InputActionDesc  action;
        InputControlType type;
    };

    struct CompositeControl: public IControl
    {
        CompositeControl(const uint32_t controls[4], uint8_t composite)
        {
            memset((void*)this, 0, sizeof(*this));
            this->composite = composite;
            memcpy(this->controls, controls, sizeof(this->controls));
            type = CONTROL_COMPOSITE;
        }
        float2   value;
        uint32_t controls[4];
        uint8_t  composite;
        uint8_t  started;
        uint8_t  performed[4];
        uint8_t  pressedVal[4];
    };

    struct FloatControl: public IControl
    {
        FloatControl(uint16_t start, uint8_t target, bool raw, bool delta)
        {
            memset((void*)this, 0, sizeof(*this));
            startButton = start;
            this->target = target;
            type = CONTROL_FLOAT;
            this->delta = (1 << (uint8_t)raw) | (uint8_t)delta;
            scale = 1;
            scaleByDT = false;
        }
        float3   value;
        float    scale;
        uint16_t startButton;
        uint8_t  target;
        uint8_t  started;
        uint8_t  performed;
        uint8_t  delta;
        uint8_t  area;
        bool     scaleByDT;
    };

    struct AxisControl: public IControl
    {
        AxisControl(uint16_t start, uint8_t target, uint8_t axis)
        {
            memset((void*)this, 0, sizeof(*this));
            startButton = start;
            this->target = target;
            axisCount = axis;
            type = CONTROL_AXIS;
        }
        float3   value;
        float3   newValue;
        uint16_t startButton;
        uint8_t  target;
        uint8_t  axisCount;
        uint8_t  started;
        uint8_t  performed;
    };

    struct VirtualJoystickControl: public IControl
    {
        float2  startPos;
        float2  currPos;
        float   outsideRadius;
        float   deadzone;
        float   scale;
        uint8_t touchIndex;
        uint8_t started;
        uint8_t performed;
        uint8_t area;
        uint8_t isPressed;
        uint8_t initialized;
        uint8_t active;
    };

    struct ComboControl: public IControl
    {
        uint16_t pressButton;
        uint16_t triggerButton;
        uint8_t  pressed;
    };

    struct GestureControl: public IControl
    {
        TouchGesture gestureType;
        uint32_t     performed; // how many fingers were processed
        uint32_t     target;    // Number of fingers required
    };

    struct FloatControlSet
    {
        FloatControl* key;
    };

    struct IControlSet
    {
        IControl* key;
    };

#if TOUCH_INPUT
    struct GestureRecognizer
    {
        struct Touch
        {
            enum State{ STARTED, HOLDING, ENDED };

            // Since we're tracking touches ourselves, we must know when to update touch data
            bool updated;
            bool moved;

            State state;
            float time;
            float velocity;
            vec2  distTraveled;
            vec2  pos0;

            uint32_t id;
            vec2     pos;
        };

        Touch    touches[MAX_INPUT_MULTI_TOUCHES];
        uint32_t activeTouches;

        uint32_t performingGesturesCount[MAX_INPUT_MULTI_TOUCHES];

        // Double tap and long press data
        vec2   lastTapPos;
        float  lastTapTime;
        Touch* longPressTouch;

        // Thresholds
        float doubleTapTimeThreshold;
        float swipeDistThreshold;
        float swipeVelocityThreshold;
        float longPressTimeThreshold;
        float movedDistThreshold;

        Touch* FindTouch(uint32_t id)
        {
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(touches); ++i)
            {
                if (touches[i].id == id)
                {
                    return &touches[i];
                }
            }

            return nullptr;
        }

        Touch* AddTouch(uint32_t id)
        {
            Touch* touch = FindTouch(id);
            if (touch != nullptr)
            {
                return touch;
            }

            for (uint32_t i = 0; i < TF_ARRAY_COUNT(touches); ++i)
            {
                if (touches[i].id == -1)
                {
                    touches[i].id = id;
                    activeTouches++;
                    return &touches[i];
                }
            }

            return nullptr;
        }

        void ReleaseTouch(uint32_t id)
        {
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(touches); i++)
            {
                if (touches[i].id == id)
                {
                    touches[i].id = -1;
                    touches[i].time = 0.0f;
                    touches[i].distTraveled = vec2(0.0f);
                    touches[i].updated = false;
                    touches[i].moved = false;
                    activeTouches--;
                    return;
                }
            }
        }
    };
#endif

    // **********************************************
    // ***** Data

    /// Maps the action mapping ID to the ActionMappingDesc
    /// C Array of stb_ds arrays
    ActionMappingDesc*    inputActionMappingIdToDesc[MAX_DEVICES] = { NULL };
    /// List of all input controls per device
    /// C Array of stb_ds arrays of stb_ds arrays of IControl*
    IControl***           controls[MAX_DEVICES] = {};
    /// This global action will be invoked everytime there is a text character typed on a physical / virtual keyboard
    GlobalInputActionDesc globalTextInputControl = { GlobalInputActionDesc::TEXT, NULL, NULL };
    /// This global action will be invoked everytime there is a button action mapping triggered
    GlobalInputActionDesc globalAnyButtonAction = { GlobalInputActionDesc::ANY_BUTTON_ACTION, NULL, NULL };
    /// List of controls which need to be canceled at the end of the frame
    /// stb_ds array of FloatControl*
    FloatControlSet*      floatDeltaControlCancelQueue = NULL;
    IControlSet*          buttonControlPerformQueue = NULL;

    IControl** controlPool[MAX_DEVICES] = { NULL };

#if TOUCH_INPUT
    GestureRecognizer gestureRecognizer;
    float2            touchPositions[gainput::TouchCount_ >> 2];
    float             touchDownTime[gainput::TouchCount_ >> 2];
#else
    float2 mousePosition;
#endif

    /// Window pointer passed by the app
    /// Input capture will be performed on this window
    WindowDesc* pWindow = NULL;

    /// Gainput Manager which lets us talk with the gainput backend
    gainput::InputManager* pInputManager = NULL;
    // gainput view which is only used for apple.
    // keep it declared for all platforms to avoid #defines in implementation
    void*                  pGainputView = NULL;

    InputDeviceType   pDeviceTypes[4 + MAX_INPUT_GAMEPADS] = {};
    gainput::DeviceId pGamepadDeviceIDs[MAX_INPUT_GAMEPADS] = {};
    gainput::DeviceId mouseDeviceID = {};
    gainput::DeviceId rawMouseDeviceID = {};
    gainput::DeviceId keyboardDeviceID = {};
    gainput::DeviceId touchDeviceID = {};

    void (*onDeviceChangeCallBack)(const char* name, bool added, int) = NULL;

    bool virtualKeyboardActive = false;
    bool inputCaptured = false;
    bool defaultCapture = false;

    // **********************************************
    // ***** Functions

    // ----- Loading

    bool Init(WindowDesc* window)
    {
        pWindow = window;

#ifdef GAINPUT_PLATFORM_GGP
        gainput::SetWindow(pWindow->handle.window);
#endif

#if TOUCH_INPUT
        memset(touchDownTime, 0, sizeof(touchDownTime));
#endif

        // Defaults
        virtualKeyboardActive = false;
        defaultCapture = true;
        inputCaptured = false;

        // Default device ids
        mouseDeviceID = gainput::InvalidDeviceId;
        rawMouseDeviceID = gainput::InvalidDeviceId;
        keyboardDeviceID = gainput::InvalidDeviceId;
        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            pGamepadDeviceIDs[i] = gainput::InvalidDeviceId;
        touchDeviceID = gainput::InvalidDeviceId;

        for (uint32_t i = 0; i < (sizeof pDeviceTypes / sizeof *pDeviceTypes); ++i)
            pDeviceTypes[i] = INPUT_DEVICE_INVALID;

        // create input manager
        pInputManager = tf_new(gainput::InputManager);
        ASSERT(pInputManager);
        pInputManager->Init((void*)pWindow->handle.window);
        pGainputView = NULL;

#if defined(_WINDOWS) || defined(XBOX)
        pInputManager->SetWindowsInstance(window->handle.window);
#elif defined(ANDROID) && !defined(QUEST_VR)
        pInputManager->SetWindowsInstance(window->handle.configuration);
#endif

#ifdef TOUCH_INPUT
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(gestureRecognizer.touches); ++i)
        {
            gestureRecognizer.touches[i].id = -1;
            gestureRecognizer.touches[i].time = 0.0f;
            gestureRecognizer.touches[i].distTraveled = vec2(0.0f);
            gestureRecognizer.touches[i].updated = false;
            gestureRecognizer.touches[i].moved = false;
            gestureRecognizer.touches[i].state = GestureRecognizer::Touch::ENDED;
            gestureRecognizer.performingGesturesCount[i] = 0;
        }

        gestureRecognizer.lastTapPos = vec2(FLT_MAX);
        gestureRecognizer.lastTapTime = FLT_MAX;
        gestureRecognizer.longPressTouch = nullptr;

        gestureRecognizer.activeTouches = 0;
        gestureRecognizer.doubleTapTimeThreshold = 0.5f;
        gestureRecognizer.swipeDistThreshold = 500.0f;
        gestureRecognizer.swipeVelocityThreshold = 1000.0f;
        gestureRecognizer.longPressTimeThreshold = 1.5f;
        gestureRecognizer.movedDistThreshold = 100.0f;
#endif

        // Used to intercept controllers connecting
        pInputManager->SetDeviceListener(this, DeviceChange);
        // create all necessary devices
        mouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>();
        rawMouseDeviceID =
            pInputManager->CreateDevice<gainput::InputDeviceMouse>(gainput::InputDevice::AutoIndex, gainput::InputDeviceMouse::DV_RAW);
        keyboardDeviceID = pInputManager->CreateDevice<gainput::InputDeviceKeyboard>();
        touchDeviceID = pInputManager->CreateDevice<gainput::InputDeviceTouch>();
        pInputManager->CreateControllers(MAX_INPUT_GAMEPADS);

        // Assign device types
        pDeviceTypes[mouseDeviceID] = InputDeviceType::INPUT_DEVICE_MOUSE;
        pDeviceTypes[rawMouseDeviceID] = InputDeviceType::INPUT_DEVICE_MOUSE;
        pDeviceTypes[keyboardDeviceID] = InputDeviceType::INPUT_DEVICE_KEYBOARD;
        pDeviceTypes[touchDeviceID] = InputDeviceType::INPUT_DEVICE_TOUCH;

        // Create control maps
        arrsetlen(controls[keyboardDeviceID], gainput::KeyCount_);
        memset(controls[keyboardDeviceID], 0, sizeof(controls[keyboardDeviceID][0]) * gainput::KeyCount_);
        arrsetlen(controls[mouseDeviceID], gainput::MouseButtonCount_);
        memset(controls[mouseDeviceID], 0, sizeof(controls[mouseDeviceID][0]) * gainput::MouseButtonCount_);
        arrsetlen(controls[rawMouseDeviceID], gainput::MouseButtonCount_);
        memset(controls[rawMouseDeviceID], 0, sizeof(controls[rawMouseDeviceID][0]) * gainput::MouseButtonCount_);
        arrsetlen(controls[touchDeviceID], gainput::TouchCount_);
        memset(controls[touchDeviceID], 0, sizeof(controls[touchDeviceID][0]) * gainput::TouchCount_);

        // Action mappings
        arrsetlen(inputActionMappingIdToDesc[mouseDeviceID], MAX_INPUT_ACTIONS);
        arrsetlen(inputActionMappingIdToDesc[rawMouseDeviceID], MAX_INPUT_ACTIONS);
        arrsetlen(inputActionMappingIdToDesc[keyboardDeviceID], MAX_INPUT_ACTIONS);
        arrsetlen(inputActionMappingIdToDesc[touchDeviceID], MAX_INPUT_ACTIONS);

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            unsigned index = DEV_PAD_START + i;

            pDeviceTypes[index] = InputDeviceType::INPUT_DEVICE_GAMEPAD;
            arrsetlen(controls[index], gainput::PadButtonMax_);
            memset(controls[index], 0, sizeof(controls[index][0]) * gainput::PadButtonMax_);

            arrsetlen(inputActionMappingIdToDesc[index], MAX_INPUT_ACTIONS);
        }

        // Clear all mappings
        RemoveActionMappings(INPUT_ACTION_MAPPING_TARGET_ALL);

        pInputManager->AddListener(this);

        return InitSubView();
    }

    void Exit()
    {
        ASSERT(pInputManager);

        RemoveActionMappings(INPUT_ACTION_MAPPING_TARGET_ALL);

        ShutdownSubView();
        pInputManager->Exit();
        tf_delete(pInputManager);

        for (uint32_t i = 0; i < MAX_DEVICES; ++i)
        {
            arrfree(inputActionMappingIdToDesc[i]);

            for (ptrdiff_t j = 0; j < arrlen(controls[i]); ++j)
                arrfree(controls[i][j]);
            arrfree(controls[i]);

            for (ptrdiff_t j = 0; j < arrlen(controlPool[i]); ++j)
                tf_free(controlPool[i][j]);
            arrfree(controlPool[i]);
        }

        hmfree(buttonControlPerformQueue);
        hmfree(floatDeltaControlCancelQueue);
    }

    // ----- Runtime

    void Update(float deltaTime, uint32_t width, uint32_t height)
    {
        ASSERT(pInputManager);

#ifdef TOUCH_INPUT
        // Long press gesture can only be triggered in Update()
        for (uint32_t i = 0; i < arrlen(controls[touchDeviceID][gainput::Touch0Down]); ++i)
        {
            IControl* control = controls[touchDeviceID][gainput::Touch0Down][i];

            if (control->type != CONTROL_GESTURE)
                continue;

            GestureControl* pControl = (GestureControl*)control;

            if (pControl->gestureType != TOUCH_GESTURE_LONG_PRESS)
                continue;

            if (gestureRecognizer.longPressTouch && !gestureRecognizer.longPressTouch->moved &&
                gestureRecognizer.longPressTouch->time > gestureRecognizer.longPressTimeThreshold &&
                gestureRecognizer.longPressTouch->state == GestureRecognizer::Touch::STARTED)
            {
                if (pControl->action.pFunction)
                {
                    InputActionContext ctx = {};
                    ctx.actionId = pControl->action.actionId;
                    ctx.pUserData = pControl->action.pUserData;
                    ctx.deviceType = pDeviceTypes[touchDeviceID];
                    ctx.pPosition = (float2*)&gestureRecognizer.longPressTouch->pos;

                    for (uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i)
                        ctx.fingerIndices[i] = gestureRecognizer.touches[i].id;

                    ctx.phase = INPUT_ACTION_PHASE_STARTED;
                    ctx.boolValue = true;
                    ctx.pCaptured = &defaultCapture;

                    pControl->action.pFunction(&ctx);
                }
            }
        }

        for (uint32_t i = 0; i < TF_ARRAY_COUNT(gestureRecognizer.touches); ++i)
        {
            gestureRecognizer.touches[i].updated = false;

            if (gestureRecognizer.touches[i].id == -1)
                continue;

            if (gestureRecognizer.touches[i].state == GestureRecognizer::Touch::ENDED)
            {
                if (gestureRecognizer.performingGesturesCount[i] > 0)
                    continue;

                gestureRecognizer.lastTapPos = gestureRecognizer.touches[i].pos;
                gestureRecognizer.lastTapTime = gestureRecognizer.touches[i].time;

                gestureRecognizer.ReleaseTouch(gestureRecognizer.touches[i].id);
                continue;
            }

            if (gestureRecognizer.touches[i].time > gestureRecognizer.longPressTimeThreshold)
            {
                gestureRecognizer.touches[i].state = GestureRecognizer::Touch::HOLDING;
            }

            if (!gestureRecognizer.touches[i].moved &&
                length(gestureRecognizer.touches[i].distTraveled) > gestureRecognizer.movedDistThreshold)
            {
                gestureRecognizer.touches[i].moved = true;
            }

            gestureRecognizer.touches[i].time += deltaTime;
        }

        gestureRecognizer.lastTapTime += deltaTime;
#endif

        for (ptrdiff_t i = 0; i < hmlen(floatDeltaControlCancelQueue); ++i)
        {
            FloatControl* pControl = floatDeltaControlCancelQueue[i].key;
            pControl->started = 0;
            pControl->performed = 0;
            pControl->value = float3(0.0f);

            InputActionContext ctx = {};
            ctx.pUserData = pControl->action.pUserData;
            ctx.phase = INPUT_ACTION_PHASE_CANCELED;
            ctx.pCaptured = &defaultCapture;
            ctx.actionId = pControl->action.actionId;
#if TOUCH_INPUT
            ctx.deviceType = INPUT_DEVICE_TOUCH;
            ctx.pPosition = &touchPositions[pControl->action.userId];
#else
            ctx.deviceType = INPUT_DEVICE_MOUSE;
            ctx.pPosition = &mousePosition;
#endif
            if (pControl->action.pFunction)
                pControl->action.pFunction(&ctx);

            if (globalAnyButtonAction.pFunction)
            {
                ctx.pUserData = globalAnyButtonAction.pUserData;
                globalAnyButtonAction.pFunction(&ctx);
            }
        }

#if TOUCH_INPUT
        for (ptrdiff_t i = 0; i < hmlen(buttonControlPerformQueue); ++i)
        {
            IControl*          pControl = buttonControlPerformQueue[i].key;
            InputActionContext ctx = {};
            ctx.pUserData = pControl->action.pUserData;
            ctx.deviceType = INPUT_DEVICE_TOUCH;
            ctx.phase = INPUT_ACTION_PHASE_UPDATED;
            ctx.pCaptured = &defaultCapture;
            ctx.actionId = pControl->action.actionId;
            ctx.pPosition = &touchPositions[pControl->action.userId];
            ctx.boolValue = true;

            if (pControl->action.pFunction)
                pControl->action.pFunction(&ctx);

            if (globalAnyButtonAction.pFunction)
            {
                ctx.pUserData = globalAnyButtonAction.pUserData;
                globalAnyButtonAction.pFunction(&ctx);
            }
        }
#endif
        hmfree(buttonControlPerformQueue);
        hmfree(floatDeltaControlCancelQueue);

        gainput::InputDeviceKeyboard* keyboard = (gainput::InputDeviceKeyboard*)pInputManager->GetDevice(keyboardDeviceID);
        if (keyboard)
        {
            uint32_t count = 0;
            wchar_t* pText = keyboard->GetTextInput(&count);
            if (count)
            {
                InputActionContext ctx = {};
                ctx.pText = pText;
                ctx.deviceType = INPUT_DEVICE_KEYBOARD;
                ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                ctx.pUserData = globalTextInputControl.pUserData;
                if (globalTextInputControl.pFunction)
                {
                    globalTextInputControl.pFunction(&ctx);
                }
            }
        }

        // update gainput manager
        pInputManager->SetDisplaySize(width, height);
        pInputManager->Update(deltaTime);

#if defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
        // this needs to be done before updating the events
        // that way current frame data will be delta after resetting mouse position
        if (inputCaptured)
        {
            ASSERT(pWindow);

            float x = 0;
            float y = 0;
            x = (pWindow->windowedRect.right - pWindow->windowedRect.left) / 2;
            y = (pWindow->windowedRect.bottom - pWindow->windowedRect.top) / 2;
            XWarpPointer(pWindow->handle.display, None, pWindow->handle.window, 0, 0, 0, 0, x, y);
            gainput::InputDevice* device = pInputManager->GetDevice(rawMouseDeviceID);
            device->WarpMouse(x, y);
            XFlush(pWindow->handle.display);
        }
#endif
    }

    // ----- Action & Control Creation

    template<typename T>
    T* AllocateControl(const gainput::DeviceId deviceId)
    {
        T* pControl = (T*)tf_calloc(1, sizeof(T));
        arrpush(controlPool[deviceId], pControl);
        return pControl;
    }

    void CreateActionForActionMapping(const ActionMappingDesc* const pActionMappingDesc, const InputActionDesc* const pActionDesc)
    {
        InputActionDesc action = *pActionDesc;

        switch (pActionMappingDesc->actionMappingDeviceTarget)
        {
        case INPUT_ACTION_MAPPING_TARGET_CONTROLLER:
        {
            const unsigned index = DEV_PAD_START + pActionMappingDesc->userId;

            switch (pActionMappingDesc->actionMappingType)
            {
            case INPUT_ACTION_MAPPING_NORMAL:
            {
                if (pActionMappingDesc->deviceButtons[0] >= GAMEPAD_BUTTON_START)
                {
                    IControl* pControl = AllocateControl<IControl>(index);
                    ASSERT(pControl);

                    pControl->type = CONTROL_BUTTON;
                    pControl->action = action;
                    arrpush(controls[index][pActionMappingDesc->deviceButtons[0]], pControl);
                }
                else // it's an axis
                {
                    // Ensure # axis is correct
                    ASSERT(pActionMappingDesc->numAxis == 1 || pActionMappingDesc->numAxis == 2);

                    AxisControl* pControl = AllocateControl<AxisControl>(index);
                    ASSERT(pControl);

                    memset((void*)pControl, 0, sizeof(*pControl));
                    pControl->type = CONTROL_AXIS;
                    pControl->action = action;
                    pControl->startButton = (uint16_t)pActionMappingDesc->deviceButtons[0];
                    pControl->axisCount = pActionMappingDesc->numAxis;
                    pControl->target = (pControl->axisCount == 2 ? (1 << 1) | 1 : 1);
                    for (uint32_t i = 0; i < pControl->axisCount; ++i)
                        arrpush(controls[index][pControl->startButton + i], pControl);
                }

                break;
            }
            case INPUT_ACTION_MAPPING_COMPOSITE:
            {
                CompositeControl* pControl = AllocateControl<CompositeControl>(index);
                ASSERT(pControl);

                memset((void*)pControl, 0, sizeof(*pControl));
                pControl->composite = pActionMappingDesc->compositeUseSingleAxis ? 2 : 4;
                pControl->controls[0] = pActionMappingDesc->deviceButtons[0];
                pControl->controls[1] = pActionMappingDesc->deviceButtons[1];
                pControl->controls[2] = pActionMappingDesc->deviceButtons[2];
                pControl->controls[3] = pActionMappingDesc->deviceButtons[3];
                pControl->type = CONTROL_COMPOSITE;
                pControl->action = action;
                for (uint32_t i = 0; i < pControl->composite; ++i)
                    arrpush(controls[index][pControl->controls[i]], pControl);

                break;
            }
            case INPUT_ACTION_MAPPING_COMBO:
            {
                ComboControl* pControl = AllocateControl<ComboControl>(index);
                ASSERT(pControl);

                pControl->type = CONTROL_COMBO;
                pControl->action = action;
                pControl->pressButton = (uint16_t)pActionMappingDesc->deviceButtons[0];
                pControl->triggerButton = (uint16_t)pActionMappingDesc->deviceButtons[1];
                arrpush(controls[index][pActionMappingDesc->deviceButtons[0]], pControl);
                arrpush(controls[index][pActionMappingDesc->deviceButtons[1]], pControl);
                break;
            }
            default:
                ASSERT(0); // should never get here
            }
            break;
        }
        case INPUT_ACTION_MAPPING_TARGET_KEYBOARD:
        {
            switch (pActionMappingDesc->actionMappingType)
            {
            case INPUT_ACTION_MAPPING_NORMAL:
            {
                // No axis available for keyboard
                IControl* pControl = AllocateControl<IControl>(keyboardDeviceID);
                ASSERT(pControl);

                pControl->type = CONTROL_BUTTON;
                pControl->action = action;
                arrpush(controls[keyboardDeviceID][pActionMappingDesc->deviceButtons[0]], pControl);

                break;
            }
            case INPUT_ACTION_MAPPING_COMPOSITE:
            {
                CompositeControl* pControl = AllocateControl<CompositeControl>(keyboardDeviceID);
                ASSERT(pControl);

                memset((void*)pControl, 0, sizeof(*pControl));
                pControl->composite = pActionMappingDesc->compositeUseSingleAxis ? 2 : 4;
                pControl->controls[0] = pActionMappingDesc->deviceButtons[0];
                pControl->controls[1] = pActionMappingDesc->deviceButtons[1];
                pControl->controls[2] = pActionMappingDesc->deviceButtons[2];
                pControl->controls[3] = pActionMappingDesc->deviceButtons[3];
                pControl->type = CONTROL_COMPOSITE;
                pControl->action = action;
                for (uint32_t i = 0; i < pControl->composite; ++i)
                    arrpush(controls[keyboardDeviceID][pControl->controls[i]], pControl);

                break;
            }
            case INPUT_ACTION_MAPPING_COMBO:
            {
                ComboControl* pControl = AllocateControl<ComboControl>(keyboardDeviceID);
                ASSERT(pControl);

                pControl->type = CONTROL_COMBO;
                pControl->action = action;
                pControl->pressButton = (uint16_t)pActionMappingDesc->deviceButtons[0];
                pControl->triggerButton = (uint16_t)pActionMappingDesc->deviceButtons[1];
                arrpush(controls[keyboardDeviceID][pActionMappingDesc->deviceButtons[0]], pControl);
                arrpush(controls[keyboardDeviceID][pActionMappingDesc->deviceButtons[1]], pControl);
                break;
            }
            default:
                ASSERT(0); // should never get here
            }
            break;
        }
        case INPUT_ACTION_MAPPING_TARGET_MOUSE:
        {
            switch (pActionMappingDesc->actionMappingType)
            {
            case INPUT_ACTION_MAPPING_NORMAL:
            {
                if (pActionMappingDesc->deviceButtons[0] < MOUSE_BUTTON_COUNT)
                {
                    // No axis available for keyboard
                    IControl* pControl = AllocateControl<IControl>(mouseDeviceID);
                    ASSERT(pControl);

                    pControl->type = CONTROL_BUTTON;
                    pControl->action = action;
                    arrpush(controls[mouseDeviceID][pActionMappingDesc->deviceButtons[0]], pControl);
                }
                else // it's an axis
                {
                    // Ensure # axis is correct
                    ASSERT(pActionMappingDesc->numAxis == 1 || pActionMappingDesc->numAxis == 2);

                    FloatControl* pControl = AllocateControl<FloatControl>(mouseDeviceID);
                    ASSERT(pControl);

                    memset((void*)pControl, 0, sizeof(*pControl));
                    pControl->type = CONTROL_FLOAT;
                    pControl->startButton = (uint16_t)pActionMappingDesc->deviceButtons[0];
                    pControl->target = (pActionMappingDesc->numAxis == 2 ? (1 << 1) | 1 : 1);
                    pControl->delta = pActionMappingDesc->delta ? (1 << 1) | 1 : 0;
                    pControl->action = action;
                    pControl->scale = pActionMappingDesc->scale;
                    pControl->scaleByDT = pActionMappingDesc->scaleByDT;

                    const gainput::DeviceId deviceId = rawMouseDeviceID; // always use raw mouse for float axis

                    for (uint32_t i = 0; i < pActionMappingDesc->numAxis; ++i)
                        arrpush(controls[deviceId][pControl->startButton + i], pControl);
                }

                break;
            }
            case INPUT_ACTION_MAPPING_COMPOSITE:
            {
                CompositeControl* pControl = AllocateControl<CompositeControl>(mouseDeviceID);
                ASSERT(pControl);

                memset((void*)pControl, 0, sizeof(*pControl));
                pControl->composite = 4;
                pControl->controls[0] = pActionMappingDesc->deviceButtons[0];
                pControl->controls[1] = pActionMappingDesc->deviceButtons[1];
                pControl->controls[2] = pActionMappingDesc->deviceButtons[2];
                pControl->controls[3] = pActionMappingDesc->deviceButtons[3];
                pControl->type = CONTROL_COMPOSITE;
                pControl->action = action;
                for (uint32_t i = 0; i < pControl->composite; ++i)
                    arrpush(controls[mouseDeviceID][pControl->controls[i]], pControl);

                break;
            }
            case INPUT_ACTION_MAPPING_COMBO:
            {
                ComboControl* pControl = AllocateControl<ComboControl>(mouseDeviceID);
                ASSERT(pControl);

                pControl->type = CONTROL_COMBO;
                pControl->action = action;
                pControl->pressButton = (uint16_t)pActionMappingDesc->deviceButtons[0];
                pControl->triggerButton = (uint16_t)pActionMappingDesc->deviceButtons[1];
                arrpush(controls[mouseDeviceID][pActionMappingDesc->deviceButtons[0]], pControl);
                arrpush(controls[mouseDeviceID][pActionMappingDesc->deviceButtons[1]], pControl);
                break;
            }
            default:
                ASSERT(0); // should never get here
            }
            break;
        }
        case INPUT_ACTION_MAPPING_TARGET_TOUCH:
        {
#if TOUCH_INPUT
            switch (pActionMappingDesc->actionMappingType)
            {
            case INPUT_ACTION_MAPPING_NORMAL:
            {
                // It's a normal tap button
                if (pActionMappingDesc->deviceButtons[0] == TOUCH_BUTTON_NONE)
                {
                    IControl* pControl = AllocateControl<IControl>(touchDeviceID);
                    ASSERT(pControl);

                    pControl->type = CONTROL_BUTTON;
                    pControl->action = action;
                    arrpush(controls[touchDeviceID][TOUCH_DOWN(pActionMappingDesc->userId)], pControl);
                }
                else // It's an axis
                {
                    // Ensure # axis is correct
                    ASSERT(pActionMappingDesc->numAxis == 1 || pActionMappingDesc->numAxis == 2);

                    FloatControl* pControl = AllocateControl<FloatControl>(touchDeviceID);
                    ASSERT(pControl);

                    memset((void*)pControl, 0, sizeof(*pControl));
                    pControl->type = CONTROL_FLOAT;
                    pControl->startButton = pActionMappingDesc->deviceButtons[0];
                    pControl->target = (pActionMappingDesc->numAxis == 2 ? (1 << 1) | 1 : 1);
                    pControl->delta = pActionMappingDesc->delta ? (1 << 1) | 1 : 0;
                    pControl->action = action;
                    pControl->scale = pActionMappingDesc->scale;
                    pControl->scaleByDT = pActionMappingDesc->scaleByDT;
                    pControl->area = pActionMappingDesc->touchScreenArea;

                    arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch1Down], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch2Down], pControl);

                    if (pActionMappingDesc->deviceButtons[0] == TOUCH_AXIS_X)
                    {
                        arrpush(controls[touchDeviceID][gainput::Touch0X], pControl);
                        arrpush(controls[touchDeviceID][gainput::Touch1X], pControl);
                        arrpush(controls[touchDeviceID][gainput::Touch2X], pControl);
                    }

                    if (pActionMappingDesc->deviceButtons[0] == TOUCH_AXIS_Y || pActionMappingDesc->numAxis > 1)
                    {
                        arrpush(controls[touchDeviceID][gainput::Touch0Y], pControl);
                        arrpush(controls[touchDeviceID][gainput::Touch1Y], pControl);
                        arrpush(controls[touchDeviceID][gainput::Touch2Y], pControl);
                    }
                }

                break;
            }
            case INPUT_ACTION_MAPPING_TOUCH_GESTURE:
            {
#ifndef NX64
                GestureControl* pControl = AllocateControl<GestureControl>(touchDeviceID);
                ASSERT(pControl);

                pControl->type = CONTROL_GESTURE;
                pControl->action = action;
                pControl->performed = 0;

                pControl->gestureType = (TouchGesture)pActionMappingDesc->deviceButtons[0];

                switch (pControl->gestureType)
                {
                case TOUCH_GESTURE_TAP:
                    arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                    pControl->target = 1;
                    break;
                case TOUCH_GESTURE_DOUBLE_TAP:
                    arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                    pControl->target = 1;
                    break;
                case TOUCH_GESTURE_PAN:
                    for (int finger = 0; finger < MAX_INPUT_MULTI_TOUCHES; finger++)
                    {
                        int idxOffset = finger * GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                        arrpush(controls[touchDeviceID][gainput::Touch0Down + idxOffset], pControl);
                        arrpush(controls[touchDeviceID][gainput::Touch0X + idxOffset], pControl);
                        arrpush(controls[touchDeviceID][gainput::Touch0Y + idxOffset], pControl);
                    }
                    pControl->target = 1;
                    break;
                case TOUCH_GESTURE_SWIPE:
                    arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch0X], pControl);
                    pControl->target = 1;
                    break;
                case TOUCH_GESTURE_PINCH:
                case TOUCH_GESTURE_ROTATE:
                    arrpush(controls[touchDeviceID][gainput::Touch0X], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch1X], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch1Down], pControl);
                    pControl->target = 2;
                    break;
                case TOUCH_GESTURE_LONG_PRESS:
                    // This specific array is looped in update()
                    arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(controls[touchDeviceID][gainput::Touch1X], pControl);
                    pControl->target = 1;
                    break;
                default:
                    ASSERT(0);
                }
#endif

                break;
            }
            case INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK:
            {
                VirtualJoystickControl* pControl = AllocateControl<VirtualJoystickControl>(touchDeviceID);
                ASSERT(pControl);

                pControl->type = CONTROL_VIRTUAL_JOYSTICK;
                pControl->action = action;
                pControl->outsideRadius = pActionMappingDesc->outsideRadius;
                pControl->deadzone = pActionMappingDesc->deadzone;
                pControl->scale = pActionMappingDesc->scale;
                pControl->touchIndex = 0xFF;
                pControl->area = pActionMappingDesc->touchScreenArea;
                arrpush(controls[touchDeviceID][gainput::Touch0Down], pControl);
                arrpush(controls[touchDeviceID][gainput::Touch0X], pControl);
                arrpush(controls[touchDeviceID][gainput::Touch0Y], pControl);
                arrpush(controls[touchDeviceID][gainput::Touch1Down], pControl);
                arrpush(controls[touchDeviceID][gainput::Touch1X], pControl);
                arrpush(controls[touchDeviceID][gainput::Touch1Y], pControl);

                break;
            }
            default:
                ASSERT(0); // should never get here
            }
#endif
            break;
        }
        default:
            ASSERT(0); // should never get here
        }
    }

    void AddInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
    {
        ASSERT(pDesc);
        ASSERT(pDesc->actionId < MAX_INPUT_ACTIONS);

        if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            ActionMappingDesc* pActionMappingDesc = &inputActionMappingIdToDesc[keyboardDeviceID][pDesc->actionId];
            if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget)
            {
                ASSERT(pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
            }

            if (pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
                CreateActionForActionMapping(pActionMappingDesc, pDesc);
        }

        if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            const unsigned index = DEV_PAD_START + pDesc->userId;

            ActionMappingDesc* pActionMappingDesc = &inputActionMappingIdToDesc[index][pDesc->actionId];
            if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget)
            {
                ASSERT(pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
            }

            if (pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
                CreateActionForActionMapping(pActionMappingDesc, pDesc);
        }

        if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            ActionMappingDesc* pActionMappingDesc = &inputActionMappingIdToDesc[mouseDeviceID][pDesc->actionId];
            if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget)
            {
                ASSERT(pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
            }

            if (pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
                CreateActionForActionMapping(pActionMappingDesc, pDesc);
        }

        if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            ActionMappingDesc* pActionMappingDesc = &inputActionMappingIdToDesc[touchDeviceID][pDesc->actionId];
            if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget)
            {
                ASSERT(pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
            }

            if (pActionMappingDesc->actionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
                CreateActionForActionMapping(pActionMappingDesc, pDesc);
        }
    }

    void RemoveInputActionControls(const InputActionDesc* pDesc, const unsigned index)
    {
        for (ptrdiff_t i = 0; i < arrlen(controls[index]); ++i)
        {
            if (arrlen(controls[index][i]) > 0)
            {
                for (ptrdiff_t j = arrlen(controls[index][i]) - 1; j >= 0; --j)
                {
                    if (controls[index][i][j]->action == *pDesc)
                    {
                        // Free is from the controls pool first and remove the entry
                        for (ptrdiff_t k = 0; k < arrlen(controlPool[index]); ++k)
                        {
                            if (controls[index][i][j] == controlPool[index][k])
                            {
                                tf_free(controlPool[index][k]);
                                arrdel(controlPool[index], k);
                                break;
                            }
                        }

                        // Then remove the entry from controls
                        arrdel(controls[index][i], j);
                    }
                }
            }
        }
    }

    void RemoveInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
    {
        ASSERT(pDesc);

        if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            RemoveInputActionControls(pDesc, DEV_PAD_START + pDesc->userId);
        }
        if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            RemoveInputActionControls(pDesc, keyboardDeviceID);
        }
        if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            RemoveInputActionControls(pDesc, mouseDeviceID);
            RemoveInputActionControls(pDesc, rawMouseDeviceID);
        }
        if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            RemoveInputActionControls(pDesc, touchDeviceID);
        }
    }

    void SetGlobalInputAction(const GlobalInputActionDesc* pDesc)
    {
        ASSERT(pDesc);
        switch (pDesc->globalInputActionType)
        {
        case GlobalInputActionDesc::ANY_BUTTON_ACTION:
            globalAnyButtonAction.pFunction = pDesc->pFunction;
            globalAnyButtonAction.pUserData = pDesc->pUserData;
            break;
        case GlobalInputActionDesc::TEXT:
            globalTextInputControl.pFunction = pDesc->pFunction;
            globalTextInputControl.pUserData = pDesc->pUserData;
            break;
        default:
            ASSERT(0); // should never get here
        }
    }

    void AddActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions,
                           const InputActionMappingDeviceTarget actionMappingTarget)
    {
        // Ensure there isn't too many actions than we can fit in memory
        ASSERT(numActions < MAX_INPUT_ACTIONS);

        // First need to reset mappings
        RemoveActionMappings(actionMappingTarget);

        // Clear transient data structures
        hmfree(buttonControlPerformQueue);
        hmfree(floatDeltaControlCancelQueue);

        for (uint32_t i = 0; i < numActions; ++i)
        {
            ActionMappingDesc* pActionMappingDesc = &actionMappings[i];
            ASSERT(pActionMappingDesc);
            ASSERT(INPUT_ACTION_MAPPING_TARGET_ALL !=
                   pActionMappingDesc->actionMappingDeviceTarget); // target cannot be INPUT_ACTION_MAPPING_TARGET_ALL in the desc

            if (pActionMappingDesc != NULL) //-V547
            {
                // Ensure action mapping ID is within acceptable range
                ASSERT(pActionMappingDesc->actionId < MAX_INPUT_ACTIONS);

                unsigned index = ~0u;

                switch (pActionMappingDesc->actionMappingDeviceTarget)
                {
                case INPUT_ACTION_MAPPING_TARGET_CONTROLLER:
                {
                    index = DEV_PAD_START + pActionMappingDesc->userId;
                    break;
                }
                case INPUT_ACTION_MAPPING_TARGET_KEYBOARD:
                {
                    index = keyboardDeviceID;
                    break;
                }
                case INPUT_ACTION_MAPPING_TARGET_MOUSE:
                {
                    index = mouseDeviceID;
                    break;
                }
                case INPUT_ACTION_MAPPING_TARGET_TOUCH:
                {
                    index = touchDeviceID;
                    // Ensure the proper action mapping type is used
                    ASSERT(INPUT_ACTION_MAPPING_NORMAL == pActionMappingDesc->actionMappingType ||
                           INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK == pActionMappingDesc->actionMappingType ||
                           INPUT_ACTION_MAPPING_TOUCH_GESTURE == pActionMappingDesc->actionMappingType);
                    break;
                }
                default:
                    ASSERT(0); // should never get here
                }

                ASSERT(index != ~0u);
                switch (pActionMappingDesc->actionMappingType)
                {
                case INPUT_ACTION_MAPPING_NORMAL:
                case INPUT_ACTION_MAPPING_COMPOSITE:
                case INPUT_ACTION_MAPPING_COMBO:
                case INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK:
                case INPUT_ACTION_MAPPING_TOUCH_GESTURE:
                {
                    ASSERT(inputActionMappingIdToDesc[index][pActionMappingDesc->actionId].actionMappingDeviceTarget ==
                           INPUT_ACTION_MAPPING_TARGET_ALL);
                    inputActionMappingIdToDesc[index][pActionMappingDesc->actionId] = *pActionMappingDesc;

                    // Register an action for UI action mappings so that the app can intercept them via the global action
                    // (GLOBAL_INPUT_ACTION_ANY_BUTTON_ACTION)
                    if (pActionMappingDesc->actionId > UISystemInputActions::UI_ACTION_START_ID_)
                    {
                        // Ensure the type is INPUT_ACTION_MAPPING_NORMAL
                        ASSERT(INPUT_ACTION_MAPPING_NORMAL == pActionMappingDesc->actionMappingType);

                        InputActionDesc actionDesc;
                        actionDesc.actionId = pActionMappingDesc->actionId;
                        actionDesc.userId = pActionMappingDesc->userId;
                        AddInputAction(&actionDesc, pActionMappingDesc->actionMappingDeviceTarget);
                    }
                    break;
                }
                default:
                    ASSERT(0); // should never get here
                }
            }
        }
    }

    void RemoveActionMappingsControls(const gainput::DeviceId deviceId)
    {
        for (ptrdiff_t j = 0; j < arrlen(controlPool[deviceId]); ++j)
            tf_free(controlPool[deviceId][j]);
        arrfree(controlPool[deviceId]);

        for (ptrdiff_t j = 0; j < arrlen(controls[deviceId]); ++j)
            arrfree(controls[deviceId][j]);
    }

    void RemoveActionMappings(const InputActionMappingDeviceTarget actionMappingTarget)
    {
        if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            {
                const unsigned index = DEV_PAD_START + i;
                memset((void*)inputActionMappingIdToDesc[index], 0, sizeof(inputActionMappingIdToDesc[index][0]) * MAX_INPUT_ACTIONS);
                RemoveActionMappingsControls(index);
                memset(controls[index], 0, sizeof(controls[index][0]) * gainput::PadButtonMax_);
            }
        }
        if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            memset((void*)inputActionMappingIdToDesc[keyboardDeviceID], 0,
                   sizeof(inputActionMappingIdToDesc[keyboardDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(keyboardDeviceID);
            memset(controls[keyboardDeviceID], 0, sizeof(controls[keyboardDeviceID][0]) * gainput::KeyCount_);
        }
        if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            memset((void*)inputActionMappingIdToDesc[mouseDeviceID], 0,
                   sizeof(inputActionMappingIdToDesc[mouseDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(mouseDeviceID);
            memset(controls[mouseDeviceID], 0, sizeof(controls[mouseDeviceID][0]) * gainput::MouseButtonCount_);

            // Need to do the same for the raw mouse device
            memset((void*)inputActionMappingIdToDesc[rawMouseDeviceID], 0,
                   sizeof(inputActionMappingIdToDesc[rawMouseDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(rawMouseDeviceID);
            memset(controls[rawMouseDeviceID], 0, sizeof(controls[rawMouseDeviceID][0]) * gainput::MouseButtonCount_);
        }
        if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            memset((void*)inputActionMappingIdToDesc[touchDeviceID], 0,
                   sizeof(inputActionMappingIdToDesc[touchDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(touchDeviceID);
            memset(controls[touchDeviceID], 0, sizeof(controls[touchDeviceID][0]) * gainput::TouchCount_);
        }
    }

    // ----- OS Input Quirks

    bool InitSubView()
    {
#ifdef __APPLE__
        if (pWindow)
        {
            void* view = pWindow->handle.window;
            if (!view)
            {
                ASSERT(false && "View is required");
                return false;
            }

#ifdef TARGET_IOS
            UIView*      mainView = (UIView*)CFBridgingRelease(view);
            GainputView* newView = [[GainputView alloc] initWithFrame:mainView.bounds inputManager:*pInputManager];
            // we want everything to resize with main view.
            [newView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleLeftMargin |
                                          UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
#else
            NSView*              mainView = (__bridge NSView*)view;
            float                retinScale = ((CAMetalLayer*)(mainView.layer)).drawableSize.width / mainView.frame.size.width;
            // Use view.window.contentLayoutRect instead of view.frame as a frame to avoid capturing inputs over title bar
            GainputMacInputView* newView = [[GainputMacInputView alloc] initWithFrame:mainView.window.contentLayoutRect
                                                                               window:mainView.window
                                                                          retinaScale:retinScale
                                                                         inputManager:*pInputManager];
            newView.nextKeyView = mainView;
            [newView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
#endif
            [mainView addSubview:newView];

#ifdef TARGET_IOS
#else
            NSWindow* window = [newView window];
            BOOL      madeFirstResponder = [window makeFirstResponder:newView];
            if (!madeFirstResponder)
                return false;
#endif

            pGainputView = (__bridge void*)newView;
        }
#endif

        return true;
    }

    void ShutdownSubView()
    {
#ifdef __APPLE__
        if (!pGainputView)
            return;

        // automatic reference counting
        // it will get deallocated.
        if (pGainputView)
        {
#ifndef TARGET_IOS
            GainputMacInputView* view = (GainputMacInputView*)CFBridgingRelease(pGainputView);
#else
            GainputView* view = (GainputView*)CFBridgingRelease(pGainputView);
#endif
            [view removeFromSuperview];
            pGainputView = NULL;
        }
#endif
    }

    bool SetEnableCaptureInput(bool enable)
    {
        ASSERT(pWindow);

        if (enable != inputCaptured)
        {
            captureCursor(pWindow, enable);
            inputCaptured = enable;

#if !defined(TARGET_IOS) && defined(__APPLE__)
            GainputMacInputView* view = (__bridge GainputMacInputView*)(pGainputView);
            [view SetMouseCapture:enable];
            view = NULL;
#endif

            return true;
        }

        return false;
    }

    void SetVirtualKeyboard(uint32_t type)
    {
        UNREF_PARAM(type);
#ifdef TARGET_IOS
        if (!pGainputView)
            return;

        if ((type > 0) != virtualKeyboardActive)
            virtualKeyboardActive = (type > 0);
        else
            return;

        GainputView* view = (__bridge GainputView*)(pGainputView);
        [view setVirtualKeyboard:type];
#elif defined(__ANDROID__)
        if ((type > 0) != virtualKeyboardActive)
        {
            virtualKeyboardActive = (type > 0);

            /* Note: native activity's API for soft input (ANativeActivity_showSoftInput & ANativeActivity_hideSoftInput) do not work.
             *       So we do it manually using JNI.
             */

            ANativeActivity* activity = pWindow->handle.activity;
            JNIEnv*          jni;
            jint             result = activity->vm->AttachCurrentThread(&jni, NULL);
            if (result == JNI_ERR)
            {
                ASSERT(0);
                return;
            }

            jclass    cls = jni->GetObjectClass(activity->clazz);
            jmethodID methodID = jni->GetMethodID(cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
            jstring   serviceName = jni->NewStringUTF("input_method");
            jobject   inputService = jni->CallObjectMethod(activity->clazz, methodID, serviceName);

            jclass inputServiceCls = jni->GetObjectClass(inputService);
            methodID = jni->GetMethodID(inputServiceCls, "toggleSoftInput", "(II)V");
            jni->CallVoidMethod(inputService, methodID, 0, 0);

            jni->DeleteLocalRef(serviceName);
            activity->vm->DetachCurrentThread();
        }
        else
            return;
#endif
    }

    // ----- Utils

    inline constexpr bool IsPointerType(gainput::DeviceId device) const
    {
#if TOUCH_INPUT
        return false;
#else
        return (device == mouseDeviceID || device == rawMouseDeviceID);
#endif
    }

    uint32_t IdToIndex(gainput::DeviceId deviceId)
    {
        uint32_t index = deviceId;

        if (index >= DEV_PAD_START)
        {
            // default to first slot
            index = DEV_PAD_START;

            for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            {
                if (pGamepadDeviceIDs[i] == deviceId)
                {
                    index += i;
                    break;
                }
            }
        }

        return index;
    }

    // ----- gainput::InputListener overrides

    bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
    {
        if (oldValue == newValue)
            return false;

        uint32_t device = IdToIndex(deviceId);

        if (arrlen(controls[device]))
        {
            InputActionContext ctx = {};
            ctx.deviceType = (uint8_t)pDeviceTypes[device];
            ctx.pCaptured = IsPointerType(device) ? &inputCaptured : &defaultCapture;
#if TOUCH_INPUT
            uint32_t touchIndex = 0;
            if (device == touchDeviceID)
            {
                touchIndex = TOUCH_USER(deviceButton);
                gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(touchDeviceID);
                touchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
                touchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
                ctx.pPosition = &touchPositions[touchIndex];

                // Reset when starting/ending touch
                if (oldValue != newValue)
                    touchDownTime[touchIndex] = 0.f;
            }
#else
            if (IsPointerType(device))
            {
                gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mouseDeviceID);
                mousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
                mousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
                ctx.pPosition = &mousePosition;

                // Scroll wheel position happens over three events
                // Movement start (delta is 0), movement (delta changes), movement end (delta is 0)
                // We only want to send the event when the delta changes
                static int32_t previousMovementWheelPosition = 0;
                static int32_t persistentScrollValue = 0;

                int32_t mouseWheelCurrentPosition = 0;
                if (deviceButton == gainput::MouseButtonWheelUp)
                    mouseWheelCurrentPosition = (int32_t)pMouse->GetFloat(gainput::MouseButtonWheelUp);
                else if (deviceButton == gainput::MouseButtonWheelDown)
                    mouseWheelCurrentPosition = (int32_t)pMouse->GetFloat(gainput::MouseButtonWheelDown);

                // Make sure delta value is based on the previous movement event
                const int32_t mouseWheelPositionDelta = mouseWheelCurrentPosition - previousMovementWheelPosition;
                if (mouseWheelCurrentPosition != 0 && mouseWheelPositionDelta != 0)
                {
                    persistentScrollValue = mouseWheelPositionDelta;
                    previousMovementWheelPosition = mouseWheelCurrentPosition;
                }

                ctx.scrollValue = persistentScrollValue;
            }
#endif
            bool executeNext = true;

            for (ptrdiff_t i = 0; i < arrlen(controls[device][deviceButton]); ++i)
            {
                IControl* control = controls[device][deviceButton][i];
                if (!executeNext)
                    return true;

                const InputControlType type = control->type;
                const InputActionDesc* pDesc = &control->action;
                ctx.pUserData = pDesc->pUserData;
                ctx.actionId = pDesc->actionId;
                ctx.userId = pDesc->userId;
                ASSERT(ctx.actionId != UINT_MAX);

                switch (type)
                {
                case CONTROL_BUTTON:
                {
                    ctx.boolValue = newValue;
                    if (newValue && !oldValue)
                    {
                        ctx.phase = INPUT_ACTION_PHASE_STARTED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        if (globalAnyButtonAction.pFunction)
                        {
                            ctx.pUserData = globalAnyButtonAction.pUserData;
                            globalAnyButtonAction.pFunction(&ctx);
                        }
#if TOUCH_INPUT
                        IControlSet val = { control };
                        hmputs(buttonControlPerformQueue, val);
#else
                        ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        if (globalAnyButtonAction.pFunction)
                        {
                            ctx.pUserData = globalAnyButtonAction.pUserData;
                            globalAnyButtonAction.pFunction(&ctx);
                        }
#endif
                    }
                    else if (oldValue && !newValue)
                    {
                        ctx.phase = INPUT_ACTION_PHASE_CANCELED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        if (globalAnyButtonAction.pFunction)
                        {
                            ctx.pUserData = globalAnyButtonAction.pUserData;
                            globalAnyButtonAction.pFunction(&ctx);
                        }
                    }
                    break;
                }
                case CONTROL_COMPOSITE:
                {
                    CompositeControl* pControl = (CompositeControl*)control;
                    uint32_t          index = 0;
                    for (; index < pControl->composite; ++index)
                        if (deviceButton == pControl->controls[index])
                            break;

                    const uint32_t axis = (index > 1) ? 1 : 0;
                    if (newValue)
                    {
                        pControl->pressedVal[index] = 1;
                        pControl->value[axis] = (float)pControl->pressedVal[axis * 2 + 0] - (float)pControl->pressedVal[axis * 2 + 1];
                    }

                    if (pControl->composite == 2)
                    {
                        ctx.floatValue = pControl->value[axis];
                    }
                    else
                    {
                        if (!pControl->value[0] && !pControl->value[1])
                            ctx.float2Value = float2(0.0f);
                        else
                            ctx.float2Value = pControl->value;
                    }

                    // Action Started
                    if (!pControl->started && !oldValue && newValue)
                    {
                        pControl->started = 1;
                        ctx.phase = INPUT_ACTION_PHASE_STARTED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                    }
                    // Action Performed
                    if (pControl->started && newValue && !pControl->performed[index])
                    {
                        pControl->performed[index] = 1;
                        ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                    }
                    // Action Canceled
                    if (oldValue && !newValue)
                    {
                        pControl->performed[index] = 0;
                        pControl->pressedVal[index] = 0;
                        bool allReleased = true;
                        for (uint8_t j = 0; j < pControl->composite; ++j)
                        {
                            if (pControl->performed[j])
                            {
                                allReleased = false;
                                break;
                            }
                        }
                        if (allReleased)
                        {
                            pControl->value = float2(0.0f);
                            pControl->started = 0;
                            ctx.float2Value = pControl->value;
                            ctx.phase = INPUT_ACTION_PHASE_CANCELED;
                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                        }
                        else if (pDesc->pFunction)
                        {
                            ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                            pControl->value[axis] = (float)pControl->pressedVal[axis * 2 + 0] - (float)pControl->pressedVal[axis * 2 + 1];
                            ctx.float2Value = pControl->value;
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        }
                    }

                    break;
                }
                // Mouse scroll is using OnDeviceButtonBool
                case CONTROL_FLOAT:
                {
                    FloatControl* pControl = (FloatControl*)control;
#if TOUCH_INPUT
                    const uint32_t fingerIdx = deviceButton / GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                    if (touchDeviceID == device)
                    {
                        if (!oldValue && newValue)
                        {
                            ASSERT(ctx.pPosition);

                            touchDownTime[touchIndex] = 0.f;

                            const float2 displaySize{ pInputManager->GetDisplayWidth(), pInputManager->GetDisplayHeight() };
                            if (!isPositionInsideScreenArea(*ctx.pPosition, (TouchScreenArea)pControl->area, displaySize))
                                break;

                            ctx.fingerIndices[0] = touchIndex;

                            if (pDesc->pFunction)
                            {
                                ctx.phase = INPUT_ACTION_PHASE_STARTED;
                                executeNext = pDesc->pFunction(&ctx) && executeNext;

                                if (globalAnyButtonAction.pFunction)
                                {
                                    ctx.pUserData = globalAnyButtonAction.pUserData;
                                    globalAnyButtonAction.pFunction(&ctx);
                                }
                            }
                        }
                        else if (oldValue && !newValue)
                        {
                            if (fingerIdx == touchIndex)
                            {
                                touchDownTime[touchIndex] = 0.f;
                                ctx.fingerIndices[0] = touchIndex;

                                pControl->started = 0;
                                pControl->performed = 0;

                                ctx.float2Value = float2(0.0f);
                                ctx.phase = INPUT_ACTION_PHASE_CANCELED;
                                ctx.actionId = pControl->action.actionId;

                                if (pDesc->pFunction)
                                    executeNext = pDesc->pFunction(&ctx) && executeNext;

                                if (globalAnyButtonAction.pFunction)
                                {
                                    ctx.pUserData = globalAnyButtonAction.pUserData;
                                    globalAnyButtonAction.pFunction(&ctx);
                                }
                            }
                        }
                    }
#endif
                    if (mouseDeviceID == device)
                    {
                        if (!oldValue && newValue)
                        {
                            ASSERT(deviceButton == gainput::MouseButtonWheelUp || deviceButton == gainput::MouseButtonWheelDown);

                            ctx.float2Value[1] = deviceButton == gainput::MouseButtonWheelUp ? 1.0f : -1.0f;

                            if (pDesc->pFunction)
                            {
                                ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                            }

                            FloatControlSet val = { pControl };
                            hmputs(floatDeltaControlCancelQueue, val);
                        }
                    }
                    break;
                }
#if TOUCH_INPUT
                case CONTROL_GESTURE:
                {
                    if (device == touchDeviceID)
                    {
                        if (!oldValue && newValue)
                        {
                            GestureRecognizer::Touch* touch = gestureRecognizer.AddTouch(touchIndex);

                            gestureRecognizer.performingGesturesCount[touchIndex]++;

                            if (!touch->updated && touch->state == GestureRecognizer::Touch::ENDED)
                            {
                                touch->updated = true;
                                touch->state = GestureRecognizer::Touch::STARTED;

                                touch->pos0 = vec2(ctx.pPosition->getX(), ctx.pPosition->getY());
                                touch->pos = touch->pos0;
                            }

                            GestureControl* pControl = (GestureControl*)control;

                            if (pControl->gestureType == TOUCH_GESTURE_LONG_PRESS)
                            {
                                gestureRecognizer.longPressTouch = touch;
                            }
                        }
                        else if (oldValue && !newValue)
                        {
                            GestureRecognizer::Touch* touch = gestureRecognizer.FindTouch(touchIndex);
                            ASSERT(touch);

                            gestureRecognizer.performingGesturesCount[touchIndex]--;

                            touch->state = GestureRecognizer::Touch::ENDED;
                            ctx.phase = INPUT_ACTION_PHASE_ENDED;

                            GestureControl* pControl = (GestureControl*)control;

                            for (uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i)
                                ctx.fingerIndices[i] = gestureRecognizer.touches[i].id;

                            // Taps
                            switch (pControl->gestureType)
                            {
                            case TOUCH_GESTURE_TAP:
                            {
                                ctx.boolValue = true;
                                ctx.pCaptured = &defaultCapture;
                                pControl->action.pFunction(&ctx);

                                break;
                            }
                            case TOUCH_GESTURE_PAN:
                            {
                                ctx.boolValue = false;
                                ctx.pCaptured = &defaultCapture;

                                ctx.fingerIndices[0] = touchIndex;
                                ctx.float2Value = { touch->pos.getX(), touch->pos.getY() };
                                pControl->action.pFunction(&ctx);

                                break;
                            }
                            case TOUCH_GESTURE_DOUBLE_TAP:
                            {
                                if (gestureRecognizer.activeTouches != pControl->target)
                                    break;

                                if (length(touch->pos - gestureRecognizer.lastTapPos) > gestureRecognizer.movedDistThreshold ||
                                    gestureRecognizer.lastTapTime > gestureRecognizer.doubleTapTimeThreshold)
                                    break;

                                ctx.boolValue = true;
                                ctx.pCaptured = &defaultCapture;

                                pControl->action.pFunction(&ctx);

                                break;
                            }
                            case TOUCH_GESTURE_SWIPE:
                            {
                                if (gestureRecognizer.activeTouches != pControl->target)
                                    break;

                                // We don't care about other touch indices

                                if (!touch)
                                    break;

                                vec2 dir = normalize(touch->distTraveled);

                                if (isnan(dir.getX()) || isnan(dir.getY()))
                                    break;

                                if (abs(dir.getX()) > abs(dir.getY()))
                                {
                                    dir.setX(sign(dir.getX()));
                                    dir.setY(0.0f);
                                }
                                else
                                {
                                    dir.setX(0.0f);
                                    dir.setY(sign(dir.getY()));
                                }

                                ctx.float4Value = { touch->distTraveled.getX(), touch->distTraveled.getY(), dir.getX(), dir.getY() };

                                ctx.pCaptured = &defaultCapture;

                                if (touch->velocity > gestureRecognizer.swipeVelocityThreshold ||
                                    abs(touch->distTraveled.getX()) > gestureRecognizer.swipeDistThreshold ||
                                    abs(touch->distTraveled.getY()) > gestureRecognizer.swipeDistThreshold)
                                {
                                    pControl->action.pFunction(&ctx);
                                }

                                break;
                            }
                            case TOUCH_GESTURE_LONG_PRESS:
                            {
                                if (gestureRecognizer.longPressTouch &&
                                    gestureRecognizer.longPressTouch->time > gestureRecognizer.longPressTimeThreshold)
                                {
                                    ctx.boolValue = false;
                                    ctx.pCaptured = &defaultCapture;

                                    pControl->action.pFunction(&ctx);
                                    gestureRecognizer.longPressTouch = nullptr;
                                }
                                break;
                            }
                            default:
                                break;
                            }
                        }
                    }

                    break;
                }
                case CONTROL_VIRTUAL_JOYSTICK:
                {
                    VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

                    if (!oldValue && newValue && !pControl->started)
                    {
                        const float2 displaySize{ pInputManager->GetDisplayWidth(), pInputManager->GetDisplayHeight() };

                        pControl->startPos = touchPositions[touchIndex];
                        if (isPositionInsideScreenArea(pControl->startPos, (TouchScreenArea)pControl->area, displaySize))
                        {
                            pControl->started = 0x3;
                            pControl->touchIndex = touchIndex;
                            pControl->currPos = pControl->startPos;

                            ctx.phase = INPUT_ACTION_PHASE_STARTED;
                            ctx.float2Value = float2(0.0f);
                            ctx.pPosition = &pControl->currPos;
                            ctx.actionId = pControl->action.actionId;

                            if (gVirtualJoystick)
                                virtualJoystickOnMove(gVirtualJoystick, virtualJoystickIndexFromArea((TouchScreenArea)pControl->area),
                                                      &ctx);

                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                        }
                        else
                        {
                            pControl->started = 0;
                            pControl->touchIndex = 0xFF;
                        }
                    }
                    else if (oldValue && !newValue)
                    {
                        if (pControl->touchIndex == touchIndex)
                        {
                            pControl->isPressed = 0;
                            pControl->touchIndex = 0xFF;
                            pControl->started = 0;
                            pControl->performed = 0;

                            ctx.float2Value = float2(0.0f);
                            ctx.pPosition = &pControl->currPos;
                            ctx.phase = INPUT_ACTION_PHASE_CANCELED;
                            ctx.actionId = pControl->action.actionId;

                            if (gVirtualJoystick)
                                virtualJoystickOnMove(gVirtualJoystick, virtualJoystickIndexFromArea((TouchScreenArea)pControl->area),
                                                      &ctx);

                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                        }
                    }
                    break;
                }
#endif
                case CONTROL_COMBO:
                {
                    ComboControl* pControl = (ComboControl*)control;
                    if (deviceButton == pControl->pressButton)
                    {
                        pControl->pressed = (uint8_t)newValue;
                    }
                    else if (pControl->pressed && oldValue && !newValue && pDesc->pFunction)
                    {
                        ctx.boolValue = true;
                        ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                        pDesc->pFunction(&ctx);
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }

        return true;
    }

    bool OnDeviceButtonFloat(float deltaTime, gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue,
                             float newValue)
    {
        float2* pPosition = NULL;

        uint32_t device = IdToIndex(deviceId);

#if TOUCH_INPUT
        bool touchJustStarted = false;

        const uint32_t touchIndex = TOUCH_USER(deviceButton);
        if (touchDeviceID == device)
        {
            // The first frame that a touch starts we get the touch position of the previous touch in oldValue,
            // for controls that use deltas we would get a huge delta. To prevent this we want to ignore the oldValue for a touch
            // that just started.
            touchJustStarted = (touchDownTime[touchIndex] == 0.f);

            const uint32_t fingerIdx = deviceButton / GAINPUT_TOUCH_BUTTONS_PER_FINGER;
            const uint32_t fingerButton = deviceButton - fingerIdx * GAINPUT_TOUCH_BUTTONS_PER_FINGER;

            gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(touchDeviceID);

            switch ((gainput::TouchButton)fingerButton)
            {
            case gainput::TouchButton::Touch0Down:
                ASSERT(false && "Handled in OnDeviceButtonBool");
                return true;
            case gainput::TouchButton::Touch0X:
                // We recive Touch0X and Touch0Y always, we only want to track one of these as elapsed time
                if (oldValue && newValue)
                    touchDownTime[touchIndex] += deltaTime;
                // fallthrough

            case gainput::TouchButton::Touch0Y:
                touchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
                touchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
                break; // We continue to send the axis event data

            case gainput::TouchButton::Touch0Pressure:
                // Pressure is the last element that Gainput notifies us about
                return true;

            default:
                ASSERT(false);
                break;
            }

            pPosition = &touchPositions[touchIndex];

            const uint32_t axisIndex = fingerButton - gainput::TouchButton::Touch0X;
            ASSERT(axisIndex < 2);
        }
#else
        FORGE_CONSTEXPR const bool touchJustStarted = false;
        if (IsPointerType(device))
        {
            gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mouseDeviceID);
            mousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
            mousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
            pPosition = &mousePosition;
        }
#endif
        ptrdiff_t deviceButtonCount = arrlen(controls[device]);
        if (deviceButtonCount > 0 && deviceButton < deviceButtonCount)
        {
            bool executeNext = true;

            for (ptrdiff_t i = 0; i < arrlen(controls[device][deviceButton]); ++i)
            {
                IControl* control = controls[device][deviceButton][i];
                if (!executeNext)
                    return true;

                const InputControlType type = control->type;
                const InputActionDesc* pDesc = &control->action;
                InputActionContext     ctx = {};
                ctx.deviceType = (uint8_t)pDeviceTypes[device];
                ctx.pUserData = pDesc->pUserData;
                ctx.pCaptured = IsPointerType(device) ? &inputCaptured : &defaultCapture;
                ctx.actionId = pDesc->actionId;
                ctx.pPosition = pPosition;
                ctx.userId = pDesc->userId;

                switch (type)
                {
                case CONTROL_FLOAT:
                {
                    FloatControl* pControl = (FloatControl*)control;
                    uint32_t      axis = (deviceButton - pControl->startButton);

#if TOUCH_INPUT
                    // We need to determine touch axis in a custom way, each finger has it's own axis value
                    if (touchDeviceID == device)
                    {
                        const uint32_t fingerIdx = deviceButton / GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                        //						if (pControl->action.userId != fingerIdx)
                        //							break; // This control does not care about this finger

                        ASSERT(pPosition);

                        const float2 displaySize{ pInputManager->GetDisplayWidth(), pInputManager->GetDisplayHeight() };
                        if (!isPositionInsideScreenArea(*pPosition, (TouchScreenArea)pControl->area, displaySize))
                            break;

                        ctx.fingerIndices[0] = fingerIdx;

                        const uint32_t deviceAxis = deviceButton - fingerIdx * GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                        ASSERT(deviceAxis == TOUCH_AXIS_X || deviceAxis == TOUCH_AXIS_Y && "CONTROL_FLOAT expects an X or Y value");
                        if (deviceAxis == TOUCH_AXIS_X)
                        {
                            axis = 0;
                        }
                        else
                        {
                            axis = 1;
                        }
                    }
#endif

                    if (pControl->delta & 0x1)
                    {
                        const float deltaValue = touchJustStarted ? 0.f : newValue - oldValue;
                        pControl->value[axis] +=
                            (axis > 0 ? -1.0f : 1.0f) * deltaValue * pControl->scale / (pControl->scaleByDT ? deltaTime : 1);
                        ctx.float3Value = pControl->value;

                        if (((pControl->started >> axis) & 0x1) == 0)
                        {
                            pControl->started |= (1 << axis);
                            if (pControl->started == pControl->target)
                            {
                                ctx.phase = INPUT_ACTION_PHASE_STARTED;

                                if (pDesc->pFunction)
                                    executeNext = pDesc->pFunction(&ctx) && executeNext;

                                if (globalAnyButtonAction.pFunction)
                                {
                                    ctx.pUserData = globalAnyButtonAction.pUserData;
                                    globalAnyButtonAction.pFunction(&ctx);
                                }
                            }

                            FloatControlSet val = { pControl };
                            hmputs(floatDeltaControlCancelQueue, val);
                        }

                        pControl->performed |= (1 << axis);

                        if (pControl->performed == pControl->target)
                        {
                            pControl->performed = 0;
                            ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;

                            if (globalAnyButtonAction.pFunction)
                            {
                                ctx.pUserData = globalAnyButtonAction.pUserData;
                                globalAnyButtonAction.pFunction(&ctx);
                            }
                        }
                    }
                    else if (pDesc->pFunction)
                    {
                        pControl->performed |= (1 << axis);
                        pControl->value[axis] = newValue;
                        if (pControl->performed == pControl->target)
                        {
                            pControl->performed = 0;
                            ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                            ctx.float3Value = pControl->value;
                            executeNext = pDesc->pFunction(&ctx) && executeNext;

                            if (globalAnyButtonAction.pFunction)
                            {
                                ctx.pUserData = globalAnyButtonAction.pUserData;
                                globalAnyButtonAction.pFunction(&ctx);
                            }
                        }
                    }
                    break;
                }
                case CONTROL_AXIS:
                {
                    AxisControl* pControl = (AxisControl*)control;

                    const uint32_t axis = (deviceButton - pControl->startButton);

                    pControl->newValue[axis] = newValue;
                    pControl->performed |= (1 << axis);

                    if (pControl->performed == pControl->target)
                    {
                        bool equal = true;
                        for (uint32_t j = 0; j < pControl->axisCount; ++j)
                            equal = equal && (pControl->value[j] == pControl->newValue[j]);

                        pControl->value = pControl->newValue;

                        ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                        ctx.float3Value = pControl->value;

                        if (!equal)
                        {
                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                            if (globalAnyButtonAction.pFunction)
                            {
                                ctx.pUserData = globalAnyButtonAction.pUserData;
                                globalAnyButtonAction.pFunction(&ctx);
                            }
                        }
                    }
                    else
                        continue;

                    pControl->performed = 0;
                    break;
                }
                case CONTROL_COMPOSITE:
                {
                    CompositeControl* pControl = (CompositeControl*)control;
                    uint32_t          index = 0;
                    for (; index < pControl->composite; ++index)
                        if (deviceButton == pControl->controls[index])
                            break;

                    const uint32_t axis = index & 1;
                    const float    prevValue = pControl->value[axis];
                    pControl->value[axis] = newValue;
                    if (newValue == prevValue)
                    {
                        continue;
                    }
                    else if (prevValue == 0.0)
                    {
                        ctx.phase = INPUT_ACTION_PHASE_STARTED;
                        pControl->pressedVal[index] = 1;
                        pControl->started = 1;
                    }
                    else if (newValue == 0.0)
                    {
                        ctx.phase = INPUT_ACTION_PHASE_CANCELED;
                        pControl->pressedVal[index] = 0;
                        pControl->performed[index] = 0;
                        bool anyPressed = false;
                        for (uint32_t j = 0; j < pControl->composite; ++j)
                        {
                            anyPressed |= pControl->pressedVal[j] != 0;
                        }
                        if (!anyPressed)
                            pControl->started = 0;
                    }
                    else
                    {
                        ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                        pControl->performed[index] = 1;
                    }
                    ctx.floatValue = pControl->value[0] - pControl->value[1];
                    executeNext = pDesc->pFunction(&ctx) && executeNext;
                    break;
                }

#if TOUCH_INPUT
                case CONTROL_GESTURE:
                {
                    const uint32_t deviceAxis = deviceButton - touchIndex * GAINPUT_TOUCH_BUTTONS_PER_FINGER;

                    GestureControl* pControl = (GestureControl*)control;

                    if (deviceAxis == TOUCH_AXIS_Y) // prevent processing two axes
                        continue;

                    if (pControl->action.pFunction)
                    {
                        GestureRecognizer::Touch* touch = gestureRecognizer.FindTouch(touchIndex);

                        if (!touch)
                            continue;

                        if (touch->state == GestureRecognizer::Touch::ENDED)
                            continue;

                        // save touch positions only while processing the first gesture
                        if (!touch->updated)
                        {
                            touch->updated = true;

                            touch->pos0 = touch->pos;
                            touch->pos = vec2(pPosition->getX(), pPosition->getY());
                            touch->distTraveled += touch->pos - touch->pos0;
                            touch->velocity = length(touch->pos - touch->pos0) / deltaTime;
                        }

                        for (uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i)
                            ctx.fingerIndices[i] = gestureRecognizer.touches[i].id;

                        switch (pControl->gestureType)
                        {
                        case TOUCH_GESTURE_PINCH:
                        {
                            if (gestureRecognizer.activeTouches != pControl->target)
                                continue;

                            GestureRecognizer::Touch* touch[2];
                            touch[0] = gestureRecognizer.FindTouch(0);
                            touch[1] = gestureRecognizer.FindTouch(1);

                            if (!touch[0] || !touch[1])
                                continue;

                            if (!touch[0]->updated || !touch[1]->updated)
                                continue;

                            float dist1 = length(touch[1]->pos - touch[0]->pos);
                            float dist0 = length(touch[1]->pos0 - touch[0]->pos0);

                            float velocity = abs(dist1 - dist0) / deltaTime;
                            float scale = dist1 / dist0;

                            if (scale < 0.1f)
                                continue;

                            ctx.float4Value = { velocity, scale, touch[1]->pos.getX() - touch[0]->pos.getX(),
                                                touch[1]->pos.getY() - touch[0]->pos.getY() };

                            ctx.pCaptured = &defaultCapture;

                            pControl->action.pFunction(&ctx);

                            break;
                        }
                        case TOUCH_GESTURE_ROTATE:
                        {
                            pControl->performed++;
                            if (pControl->performed != pControl->target)
                                break;
                            pControl->performed = 0;

                            if (gestureRecognizer.activeTouches != pControl->target)
                                break;

                            GestureRecognizer::Touch* touch[2];
                            touch[0] = gestureRecognizer.FindTouch(0);
                            touch[1] = gestureRecognizer.FindTouch(1);

                            if (!touch[0] || !touch[1])
                                continue;

                            vec2 v1 = touch[1]->pos - touch[0]->pos;
                            vec2 v0 = touch[1]->pos0 - touch[0]->pos0;

                            float velocity = abs(length(v1) - length(v0)) / deltaTime;
                            float rotation = atan2f(v0.getX() * v1.getY() - v0.getY() * v1.getX(), dot(v0, v1));

                            float scale = length(v1) / length(v0);

                            if (scale < 0.1f)
                                continue;

                            ctx.float4Value = { velocity, rotation, touch[1]->pos.getX() - touch[0]->pos.getX(),
                                                touch[1]->pos.getY() - touch[0]->pos.getY() };

                            ctx.pCaptured = &defaultCapture;

                            pControl->action.pFunction(&ctx);

                            break;
                        }
                        case TOUCH_GESTURE_PAN:
                        {
                            pControl->performed++;
                            if (pControl->performed != pControl->target)
                                break;

                            pControl->performed = 0;

                            GestureRecognizer::Touch* touch;
                            touch = gestureRecognizer.FindTouch(touchIndex);

                            if (!touch)
                                continue;

                            ctx.fingerIndices[0] = touchIndex;
                            ctx.float2Value = {
                                touchPositions[touchIndex][0] - touch->pos.getX(),
                                touchPositions[touchIndex][1] - touch->pos.getY(),
                            };

                            touch->pos.setX(touchPositions[touchIndex][0]);
                            touch->pos.setY(touchPositions[touchIndex][1]);

                            ctx.phase =
                                touch->state == GestureRecognizer::Touch::STARTED ? INPUT_ACTION_PHASE_STARTED : INPUT_ACTION_PHASE_UPDATED;
                            ctx.boolValue = true;
                            ctx.pCaptured = &defaultCapture;

                            if (touch->state == GestureRecognizer::Touch::STARTED)
                                touch->state = GestureRecognizer::Touch::HOLDING;

                            pControl->action.pFunction(&ctx);

                            break;
                        }
                        default:
                            break;
                        }
                    }

                    break;
                }
                case CONTROL_VIRTUAL_JOYSTICK:
                {
                    VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

                    const uint32_t axis = TOUCH_AXIS(deviceButton);

                    if (!pControl->started || TOUCH_USER(deviceButton) != pControl->touchIndex)
                        continue;

                    pControl->performed |= (1 << axis);
                    pControl->currPos[axis] = newValue;
                    if (pControl->performed == 0x3)
                    {
                        // Calculate the new joystick positions
                        vec2  delta = f2Tov2(pControl->currPos - pControl->startPos);
                        float halfRad = (pControl->outsideRadius * 0.5f) - pControl->deadzone;
                        if (length(delta) > halfRad)
                            pControl->currPos = pControl->startPos + halfRad * v2ToF2(normalize(delta));

                        ctx.phase = INPUT_ACTION_PHASE_UPDATED;
                        float2 dir = ((pControl->currPos - pControl->startPos) / halfRad) * pControl->scale;
                        ctx.float2Value = float2(dir[0], -dir[1]);
                        ctx.pPosition = &pControl->currPos;
                        ctx.actionId = pControl->action.actionId;
                        ctx.fingerIndices[0] = pControl->touchIndex;

                        if (gVirtualJoystick)
                            virtualJoystickOnMove(gVirtualJoystick, virtualJoystickIndexFromArea((TouchScreenArea)pControl->area), &ctx);

                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                    }
                    break;
                }
#endif
                default:
                    break;
                }
            }
        }

        return true;
    }

    bool OnDeviceButtonGesture(float deltaTime, gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton,
                               const struct gainput::GestureChange& gesture)
    {
        UNREF_PARAM(deltaTime);
        UNREF_PARAM(deviceId);
        UNREF_PARAM(deviceButton);
        UNREF_PARAM(gesture);
        // uint32_t device = IdToIndex(deviceId);
        return true;
    }

    int GetPriority() const { return 0; }

    // ----- GamePad Utils

    static void DeviceChange(void* metadata, gainput::DeviceId deviceId, gainput::InputDevice* device, bool doAdd)
    {
        InputSystemImpl* sys = (InputSystemImpl*)metadata;

        if (doAdd)
            sys->AddGamepad(deviceId, device);
        else
            sys->RemoveGamepad(deviceId, device);
    }

    void AddGamepad(gainput::DeviceId deviceId, gainput::InputDevice* device)
    {
        if (device->GetType() != gainput::InputDevice::DeviceType::DT_PAD)
            return;

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            if (pGamepadDeviceIDs[i] == gainput::InvalidDeviceId)
            {
                pGamepadDeviceIDs[i] = deviceId;

                if (onDeviceChangeCallBack)
                    onDeviceChangeCallBack(((gainput::InputDevicePad*)device)->GetDeviceName(), true, i);

                break;
            }
        }
    }

    void RemoveGamepad(gainput::DeviceId deviceId, gainput::InputDevice* device)
    {
        if (device->GetType() != gainput::InputDevice::DeviceType::DT_PAD)
            return;

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            if (pGamepadDeviceIDs[i] == deviceId)
            {
                if (onDeviceChangeCallBack)
                    onDeviceChangeCallBack(((gainput::InputDevicePad*)device)->GetDeviceName(), false, i);

                pGamepadDeviceIDs[i] = gainput::InvalidDeviceId;

                break;
            }
        }
    }

    void SetDeadZone(unsigned gamePadIndex, float deadZoneSize)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonL3, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonR3, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonL2, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonR2, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonLeftStickX, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonLeftStickY, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonRightStickX, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonRightStickY, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonAxis4, deadZoneSize);
        pDevicePad->SetDeadZone(gainput::PadButton::PadButtonAxis5, deadZoneSize);
    }

    const char* GetGamePadName(unsigned gamePadIndex)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS)
            return "Incorrect gamePadIndex";
        if (pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return "GamePad Disconnected";
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        return pDevicePad->GetDeviceName();
    }

    bool GamePadConnected(unsigned gamePadIndex)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return false;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        return pDevicePad->IsAvailable();
    }

    bool SetRumbleEffect(unsigned gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms, bool vibrateTouchDevice)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return false;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        return pDevicePad->SetRumbleEffect(left_motor, right_motor, duration_ms, vibrateTouchDevice);
    }

    void SetLEDColor(unsigned gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        pDevicePad->SetLEDColor(r, g, b);
    }

    void setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int))
    {
        onDeviceChangeCallBack = onDeviceChnageCallBack;

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            if (pGamepadDeviceIDs[i] != gainput::InvalidDeviceId)
            {
                gainput::InputDevice* device = pInputManager->GetDevice(pGamepadDeviceIDs[i]);

                if (onDeviceChangeCallBack)
                    onDeviceChangeCallBack(((gainput::InputDevicePad*)device)->GetDeviceName(), true, i);

                break;
            }
        }
    }
};
#endif

/**********************************************/
// Interface
/**********************************************/

#ifdef ENABLE_FORGE_INPUT
static InputSystemImpl* pInputSystem = NULL;

#if (defined(_WINDOWS) && !defined(XBOX)) || (defined(__APPLE__) && !defined(TARGET_IOS))
static void ResetInputStates()
{
    pInputSystem->pInputManager->ClearAllStates(pInputSystem->mouseDeviceID);
    pInputSystem->pInputManager->ClearAllStates(pInputSystem->keyboardDeviceID);
    for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
    {
        pInputSystem->pInputManager->ClearAllStates(pInputSystem->pGamepadDeviceIDs[i]);
    }
}
#endif

#endif

int32_t InputSystemHandleMessage(WindowDesc* pWindow, void* msg)
{
    UNREF_PARAM(msg);
    UNREF_PARAM(pWindow);
#ifdef ENABLE_FORGE_INPUT

    if (pInputSystem == nullptr)
    {
        return 0;
    }
#if defined(_WINDOWS) && !defined(XBOX)
    pInputSystem->pInputManager->HandleMessage(*(MSG*)msg);
    if ((*(MSG*)msg).message == WM_ACTIVATEAPP && (*(MSG*)msg).wParam == WA_INACTIVE)
    {
        ResetInputStates();
    }
#elif defined(__APPLE__) && !defined(TARGET_IOS)
    if (msg)
    {
        NSNotificationName name = ((__bridge NSNotification*)msg).name;
        // Reset input states when we lose focus
        if (name == NSWindowDidBecomeMainNotification || name == NSWindowDidResignMainNotification ||
            name == NSWindowDidResignKeyNotification)
        {
            ResetInputStates();
        }
    }
#elif defined(__ANDROID__) && !defined(QUEST_VR)
    return pInputSystem->pInputManager->HandleInput((AInputEvent*)msg, pWindow->handle.activity);
#elif defined(__linux__) && !defined(GAINPUT_PLATFORM_GGP) && !defined(QUEST_VR)
    pInputSystem->pInputManager->HandleEvent(*(XEvent*)msg);
#endif
#endif

    return 0;
}

bool initInputSystem(InputSystemDesc* pDesc)
{
#ifdef ENABLE_FORGE_INPUT

    ASSERT(pDesc);
    ASSERT(pDesc->pWindow);

    pInputSystem = tf_new(InputSystemImpl);

    setCustomMessageProcessor(InputSystemHandleMessage);

    bool success = pInputSystem->Init(pDesc->pWindow);

#if TOUCH_INPUT
    if (pDesc->pJoystickTexture)
    {
        ASSERT(pDesc->pRenderer);
        VirtualJoystickDesc joystickDesc = {};
        joystickDesc.pRenderer = pDesc->pRenderer;
        joystickDesc.pJoystickTexture = pDesc->pJoystickTexture;
        initVirtualJoystick(&joystickDesc, &gVirtualJoystick);
    }
#endif

    addDefaultActionMappings();

    return success;
#else
    return false;
#endif
}

void exitInputSystem()
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

#if TOUCH_INPUT
    exitVirtualJoystick(&gVirtualJoystick);
#endif

    setCustomMessageProcessor(nullptr);

    pInputSystem->Exit();
    tf_delete(pInputSystem);
    pInputSystem = NULL;
#endif
}

void updateInputSystem(float deltaTime, uint32_t width, uint32_t height)
{
    UNREF_PARAM(deltaTime);
    UNREF_PARAM(width);
    UNREF_PARAM(height);
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
#if !defined(AUTOMATED_TESTING)
    pInputSystem->Update(deltaTime, width, height);
#endif
#endif
}

void addInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->AddInputAction(pDesc, actionMappingTarget);
#endif
}

void removeInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->RemoveInputAction(pDesc, actionMappingTarget);
#endif
}

void setGlobalInputAction(const GlobalInputActionDesc* pDesc)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->SetGlobalInputAction(pDesc);
#endif
}

void addActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions,
                       const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    return pInputSystem->AddActionMappings(actionMappings, numActions, actionMappingTarget);
#endif
}

void removeActionMappings(const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    return pInputSystem->RemoveActionMappings(actionMappingTarget);
#endif
}

void addDefaultActionMappings()
{
    ActionMappingDesc actionMappingsArr[] = {
        // Camera actions
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::TRANSLATE_CAMERA,
          { KeyboardButton::KEYBOARD_BUTTON_D, KeyboardButton::KEYBOARD_BUTTON_A, KeyboardButton::KEYBOARD_BUTTON_W,
            KeyboardButton::KEYBOARD_BUTTON_S } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::TRANSLATE_CAMERA,
          { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
          2 },
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
          { KeyboardButton::KEYBOARD_BUTTON_E, KeyboardButton::KEYBOARD_BUTTON_Q },
          2,
          1,
          0,
          0,
          0,
          0.0f,
          AREA_LEFT,
          false,
          true },
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
          { GamepadButton::GAMEPAD_BUTTON_AXIS_5, GamepadButton::GAMEPAD_BUTTON_AXIS_4 },
          2,
          1,
          0,
          0,
          0,
          0.0f,
          AREA_LEFT,
          false,
          true },
        { INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK,
          INPUT_ACTION_MAPPING_TARGET_TOUCH,
          DefaultInputActions::TRANSLATE_CAMERA,
          {},
          1,
          1,
          0,
          20.f,
          200.f,
          1.f,
          AREA_LEFT },
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::ROTATE_CAMERA,
          { KeyboardButton::KEYBOARD_BUTTON_L, KeyboardButton::KEYBOARD_BUTTON_J, KeyboardButton::KEYBOARD_BUTTON_I,
            KeyboardButton::KEYBOARD_BUTTON_K } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::ROTATE_CAMERA,
          { GamepadButton::GAMEPAD_BUTTON_RIGHT_STICK_X },
          2 },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::ROTATE_CAMERA,
          { MouseButton::MOUSE_BUTTON_AXIS_X },
          2,
          1,
          0,
          0,
          0,
          0.001f,
          AREA_LEFT,
          true },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_TOUCH,
          DefaultInputActions::ROTATE_CAMERA,
          { TouchButton::TOUCH_AXIS_X },
          2,
          1,
          0,
          20.f,
          200.f,
          0.2f,
          AREA_RIGHT },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::CAPTURE_INPUT,
          { MouseButton::MOUSE_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::RESET_CAMERA,
          { KeyboardButton::KEYBOARD_BUTTON_SPACE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::RESET_CAMERA,
          { GamepadButton::GAMEPAD_BUTTON_Y } },

        // Profile data / toggle fullscreen / exit actions
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::DUMP_PROFILE_DATA,
          { KeyboardButton::KEYBOARD_BUTTON_F3 } },
        { INPUT_ACTION_MAPPING_COMBO,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::DUMP_PROFILE_DATA,
          { GamepadButton::GAMEPAD_BUTTON_START, GamepadButton::GAMEPAD_BUTTON_B } },
        { INPUT_ACTION_MAPPING_COMBO,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::TOGGLE_FULLSCREEN,
          { KeyboardButton::KEYBOARD_BUTTON_ALT_L, KeyboardButton::KEYBOARD_BUTTON_RETURN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::EXIT,
          { KeyboardButton::KEYBOARD_BUTTON_ESCAPE } },
        { INPUT_ACTION_MAPPING_COMBO,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::RELOAD_SHADERS,
          { KeyboardButton::KEYBOARD_BUTTON_CTRL_L, KeyboardButton::KEYBOARD_BUTTON_S } },

        // UI specific actions
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_TAB,
          { KeyboardButton::KEYBOARD_BUTTON_TAB } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_LEFT_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_RIGHT_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_RIGHT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_UP_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_DOWN_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_PAGE_UP,
          { KeyboardButton::KEYBOARD_BUTTON_PAGE_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_PAGE_DOWN,
          { KeyboardButton::KEYBOARD_BUTTON_PAGE_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_HOME,
          { KeyboardButton::KEYBOARD_BUTTON_HOME } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_END,
          { KeyboardButton::KEYBOARD_BUTTON_END } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_INSERT,
          { KeyboardButton::KEYBOARD_BUTTON_INSERT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_DELETE,
          { KeyboardButton::KEYBOARD_BUTTON_DELETE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_BACK_SPACE,
          { KeyboardButton::KEYBOARD_BUTTON_BACK_SPACE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SPACE,
          { KeyboardButton::KEYBOARD_BUTTON_SPACE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ENTER,
          { KeyboardButton::KEYBOARD_BUTTON_RETURN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ESCAPE,
          { KeyboardButton::KEYBOARD_BUTTON_ESCAPE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_CONTROL_L,
          { KeyboardButton::KEYBOARD_BUTTON_CTRL_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_CONTROL_R,
          { KeyboardButton::KEYBOARD_BUTTON_CTRL_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SHIFT_L,
          { KeyboardButton::KEYBOARD_BUTTON_SHIFT_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SHIFT_R,
          { KeyboardButton::KEYBOARD_BUTTON_SHIFT_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ALT_L,
          { KeyboardButton::KEYBOARD_BUTTON_ALT_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ALT_R,
          { KeyboardButton::KEYBOARD_BUTTON_ALT_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SUPER_L,
          { KeyboardButton::KEYBOARD_BUTTON_SUPER_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SUPER_R,
          { KeyboardButton::KEYBOARD_BUTTON_SUPER_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_A,
          { KeyboardButton::KEYBOARD_BUTTON_A } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_C,
          { KeyboardButton::KEYBOARD_BUTTON_C } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_V,
          { KeyboardButton::KEYBOARD_BUTTON_V } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_X,
          { KeyboardButton::KEYBOARD_BUTTON_X } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_Y,
          { KeyboardButton::KEYBOARD_BUTTON_Y } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_Z,
          { KeyboardButton::KEYBOARD_BUTTON_Z } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_F2,
          { KeyboardButton::KEYBOARD_BUTTON_F2 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_LEFT,
          { MouseButton::MOUSE_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_TOUCH, DefaultInputActions::UI_MOUSE_LEFT },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_RIGHT,
          { MouseButton::MOUSE_BUTTON_RIGHT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_MIDDLE,
          { MouseButton::MOUSE_BUTTON_MIDDLE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_SCROLL_UP,
          { MouseButton::MOUSE_BUTTON_WHEEL_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_SCROLL_DOWN,
          { MouseButton::MOUSE_BUTTON_WHEEL_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TOGGLE_UI,
          { GamepadButton::GAMEPAD_BUTTON_R3 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_NAV_TOGGLE_UI,
          { KeyboardButton::KEYBOARD_BUTTON_F1 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_ACTIVATE,
          { GamepadButton::GAMEPAD_BUTTON_A } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_CANCEL,
          { GamepadButton::GAMEPAD_BUTTON_B } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_INPUT,
          { GamepadButton::GAMEPAD_BUTTON_Y } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_MENU,
          { GamepadButton::GAMEPAD_BUTTON_X } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_LEFT,
          { GamepadButton::GAMEPAD_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_RIGHT,
          { GamepadButton::GAMEPAD_BUTTON_RIGHT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_UP,
          { GamepadButton::GAMEPAD_BUTTON_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_DOWN,
          { GamepadButton::GAMEPAD_BUTTON_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_SCROLL_MOVE_WINDOW,
          { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
          2 },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_FOCUS_PREV,
          { GamepadButton::GAMEPAD_BUTTON_L1 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_FOCUS_NEXT,
          { GamepadButton::GAMEPAD_BUTTON_R1 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_SLOW,
          { GamepadButton::GAMEPAD_BUTTON_L2 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_FAST,
          { GamepadButton::GAMEPAD_BUTTON_R2 } }
    };

    addActionMappings(actionMappingsArr, TF_ARRAY_COUNT(actionMappingsArr), INPUT_ACTION_MAPPING_TARGET_ALL);
}

bool setEnableCaptureInput(bool enable)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    return pInputSystem->SetEnableCaptureInput(enable);
#else
    return false;
#endif
}

void setVirtualKeyboard(uint32_t type)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    pInputSystem->SetVirtualKeyboard(type);
#endif
}

void setDeadZone(unsigned gamePadIndex, float deadZoneSize)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    pInputSystem->SetDeadZone(gamePadIndex, deadZoneSize);
#endif
}

const char* getGamePadName(int gamePadIndex)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    return pInputSystem->GetGamePadName(gamePadIndex);
#else
    return nullptr;
#endif
}

bool gamePadConnected(int gamePadIndex)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    return pInputSystem->GamePadConnected(gamePadIndex);
#else
    return false;
#endif
}

bool setRumbleEffect(int gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    // this is used only for mobile phones atm.
    // allows us to vibrate the actual phone instead of the connected gamepad.
    // if a gamepad is also connected, the app can decide which device should get the vibration
    bool vibrateDeviceInsteadOfPad = false;
    if (gamePadIndex == BUILTIN_DEVICE_HAPTICS)
    {
        vibrateDeviceInsteadOfPad = true;
        gamePadIndex = 0;
    }

    return pInputSystem->SetRumbleEffect(gamePadIndex, left_motor, right_motor, duration_ms, vibrateDeviceInsteadOfPad);
#else
    return false;
#endif
}

void setLEDColor(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    pInputSystem->SetLEDColor(gamePadIndex, r, g, b);
#endif
}

void setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int gamepadIndex))
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->setOnDeviceChangeCallBack(onDeviceChnageCallBack);
#endif
}
