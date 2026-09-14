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

#include "Application/IFont.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"

#include "RHI/IGraphics.h"

// include Fontstash (should be after MemoryTracking so that it also detects memory free/remove in fontstash)
#define FONTSTASH_IMPLEMENTATION
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>
#include <ThirdParty/stb/stb_ds.h>
#include <ThirdParty/bstrlib/bstrlib.h>
#include <ThirdParty/Fontstash/src/fontstash.h>

#include "RHI/IGraphics.h"
#include "Resources/IResourceLoader.h"

#include "RHI/RingBuffer.h"

#include "Core/IMemory.h"

#ifdef ENABLE_FORGE_FONTS

struct Fontstash
{
    // FONS
    FONScontext* pContext;
    // stb_ds dynamic arrays
    void**       fontBuffers;
    uint32_t*    fontBufferSizes;
    float        fontMaxSize;
    uint32_t     width;
    uint32_t     height;

    // Renderer
    Renderer*      pRenderer;
    Texture*       pAtlasTexture;
    Shader*        pShaders[2];
    RootSignature* pRootSignature;
    Pipeline*      pPipelines[2];
    Sampler*       pDefaultSampler;
    GPURingBuffer  uniformRingBuffer;
    GPURingBuffer  meshRingBuffer;
    uint32_t       rootConstantIndex;

    // Fontstash generation
    const uint8_t* pPixels;
    bool           updateTexture;

    // Render size
    float2 scaleBias;
    float2 dpiScale;
    float  dpiScaleMin;

    bool renderInitialized;

#if defined(TARGET_IOS) || defined(ANDROID)
    static const int TextureAtlasDimension = 512;
#elif defined(XBOX)
    static const int TextureAtlasDimension = 1024;
#else // PC / LINUX / MAC
    static const int TextureAtlasDimension = 2048;
#endif
};

struct FontstashDrawData
{
    CameraMatrix projView;
    mat4         worldMat;
    Cmd*         pCmd;
    bool         text3D;
};

static Fontstash gFontstash = {};

// --  FONS renderer implementation --
static int fonsImplementationGenerateTexture(void* userPtr, int width, int height)
{
    UNREF_PARAM(userPtr);
    gFontstash.width = width;
    gFontstash.height = height;
    gFontstash.updateTexture = true;
    return 1;
}

static void fonsImplementationModifyTexture(void* userPtr, int* rect, const unsigned char* data)
{
    UNREF_PARAM(userPtr);
    UNREF_PARAM(rect);
    gFontstash.pPixels = data;
    gFontstash.updateTexture = true;
}

static void fonsImplementationRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
    if (!gFontstash.pAtlasTexture)
    {
        return;
    }

    FontstashDrawData* draw = (FontstashDrawData*)userPtr;
    Cmd*               pCmd = draw->pCmd;

    if (gFontstash.updateTexture)
    {
        // #TODO: Investigate - Causes hang on low-mid end Android phones (tested on Samsung Galaxy A50s)
#ifndef __ANDROID__
        waitQueueIdle(pCmd->pQueue);
#endif
        TextureUpdateDesc updateDesc = { gFontstash.pAtlasTexture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        beginUpdateResource(&updateDesc);
        TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
        for (uint32_t r = 0; r < subresource.rowCount; ++r)
        {
            memcpy(subresource.pMappedData + r * subresource.dstRowStride, gFontstash.pPixels + r * subresource.srcRowStride,
                   subresource.srcRowStride);
        }
        endUpdateResource(&updateDesc);

        gFontstash.updateTexture = false;
    }

    GPURingBufferOffset buffer = getGPURingBufferOffset(&gFontstash.meshRingBuffer, nverts * sizeof(float4));
    BufferUpdateDesc    update = { buffer.pBuffer, buffer.offset };
    beginUpdateResource(&update);
    float4* vtx = (float4*)update.pMappedData;
    // build vertices
    for (int impl = 0; impl < nverts; impl++)
    {
        float4 vert = { verts[impl * 2 + 0], verts[impl * 2 + 1], tcoords[impl * 2 + 0], tcoords[impl * 2 + 1] };
        memcpy((void*)&vtx[impl], &vert, sizeof(vert));
    }
    endUpdateResource(&update);

    // extract color
    float4 color = unpackA8B8G8R8_SRGB(*colors);

    uint32_t  pipelineIndex = draw->text3D ? 1 : 0;
    Pipeline* pPipeline = gFontstash.pPipelines[pipelineIndex];
    ASSERT(pPipeline);

    cmdBindPipeline(pCmd, pPipeline);

    struct UniformData
    {
        float4 color;
        float2 scaleBias;
        uint32_t textureIndex;
        uint32_t samplerIndex;
        uint32_t uniformIndex;
        uint32_t uniformOffset;
    } data = {};

    data.color = color;
    data.scaleBias = gFontstash.scaleBias;
    data.textureIndex = getTextureSrvIndex(gFontstash.pAtlasTexture);
    data.samplerIndex = getSamplerIndex(gFontstash.pDefaultSampler);

    if (draw->text3D)
    {
        CameraMatrix mvp = (draw->projView * draw->worldMat);
        data.color = color;
        data.scaleBias.x = -data.scaleBias.x;

        GPURingBufferOffset uniformBlock = getGPURingBufferOffset(&gFontstash.uniformRingBuffer, sizeof(mvp));
        BufferUpdateDesc    updateDesc = { uniformBlock.pBuffer, uniformBlock.offset };
        beginUpdateResource(&updateDesc);
        memcpy(updateDesc.pMappedData, &mvp, sizeof(mvp));
        endUpdateResource(&updateDesc);

        const uint32_t stride = sizeof(float4);
        data.uniformIndex = getBufferSrvIndex(uniformBlock.pBuffer);
        data.uniformOffset = (uint32_t)uniformBlock.offset;
        cmdBindPushConstants(pCmd, gFontstash.pRootSignature, gFontstash.rootConstantIndex, &data);
        cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.offset);
        cmdDraw(pCmd, nverts, 0);
    }
    else
    {
        const uint32_t stride = sizeof(float4);
        cmdBindPushConstants(pCmd, gFontstash.pRootSignature, gFontstash.rootConstantIndex, &data);
        cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.offset);
        cmdDraw(pCmd, nverts, 0);
    }
}

void fonsImplementationRemoveTexture(void*) {}
#endif

bool platformInitFontSystem()
{
#ifdef ENABLE_FORGE_FONTS
    float          dpiScale[2] = {};
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    gFontstash.dpiScale.x = dpiScale[0];
    gFontstash.dpiScale.y = dpiScale[1];

    gFontstash.dpiScaleMin = min(gFontstash.dpiScale.x, gFontstash.dpiScale.y);

    gFontstash.width = gFontstash.TextureAtlasDimension * (int)ceilf(gFontstash.dpiScale.x);
    gFontstash.height = gFontstash.TextureAtlasDimension * (int)ceilf(gFontstash.dpiScale.y);
    gFontstash.fontMaxSize = min(gFontstash.width, gFontstash.height) / 10.0f; // see fontstash.h, line 1271, for fontSize calculation

    // create FONS context
    FONSparams params = {};
    params.width = gFontstash.width;
    params.height = gFontstash.height;
    params.flags = (unsigned char)FONS_ZERO_TOPLEFT;
    params.renderCreate = fonsImplementationGenerateTexture;
    params.renderUpdate = fonsImplementationModifyTexture;
    params.renderDelete = fonsImplementationRemoveTexture;
    params.renderDraw = fonsImplementationRenderText;
    gFontstash.pContext = fonsCreateInternal(&params);

    return gFontstash.pContext != NULL;
#else
    return true;
#endif
}

void platformExitFontSystem()
{
#ifdef ENABLE_FORGE_FONTS
    // unload font buffers
    for (ptrdiff_t i = 0; i < arrlen(gFontstash.fontBuffers); ++i)
    {
        tf_free(gFontstash.fontBuffers[i]);
    }
    arrfree(gFontstash.fontBuffers);
    // unload font buffer sizes
    arrfree(gFontstash.fontBufferSizes);

    // unload fontstash context
    fonsDeleteInternal(gFontstash.pContext);
    gFontstash = {};
#endif
}

bool initFontSystem(FontSystemDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(!gFontstash.renderInitialized);

    gFontstash.pRenderer = pDesc->pRenderer;

    // create image
    TextureDesc desc = {};
    desc.arraySize = 1;
    desc.depth = 1;
    desc.descriptors = DESCRIPTOR_TYPE_TEXTURE;
    desc.format = hz::Format::R8_UNORM;
    desc.height = gFontstash.height;
    desc.mipLevels = 1;
    desc.sampleCount = SAMPLE_COUNT_1;
    desc.startState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    desc.width = gFontstash.width;
    desc.pName = "Fontstash Texture";
    TextureLoadDesc loadDesc = {};
    loadDesc.ppTexture = &gFontstash.pAtlasTexture;
    loadDesc.pDesc = &desc;
    addResource(&loadDesc, NULL);

    /************************************************************************/
    // Rendering resources
    /************************************************************************/
    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(gFontstash.pRenderer, &samplerDesc, &gFontstash.pDefaultSampler);

    const BufferDesc uniformDesc = { .size = 65536,
                                     .elementCount = 65536 / 4,
                                     .memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                     .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT,
                                     .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW };
    addGPURingBuffer(gFontstash.pRenderer, &uniformDesc, &gFontstash.uniformRingBuffer);

    BufferDesc vbDesc = {};
    vbDesc.descriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.size = pDesc->fontstashRingSizeBytes;
    vbDesc.flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    addGPURingBuffer(gFontstash.pRenderer, &vbDesc, &gFontstash.meshRingBuffer);
    /************************************************************************/
    /************************************************************************/
    gFontstash.renderInitialized = true;
#endif
    return true;
}

void exitFontSystem()
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.renderInitialized);

    removeResource(gFontstash.pAtlasTexture);

    removeGPURingBuffer(&gFontstash.meshRingBuffer);
    removeGPURingBuffer(&gFontstash.uniformRingBuffer);
    removeSampler(gFontstash.pRenderer, gFontstash.pDefaultSampler);

    gFontstash.renderInitialized = false;
#endif
}

void loadFontSystem(const FontSystemLoadDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    if (pDesc->loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        if (pDesc->loadType & RELOAD_TYPE_SHADER)
        {
            ShaderLoadDesc text2DShaderDesc = {};
            text2DShaderDesc.stages[0] = { "fontstash2D.vert" };
            text2DShaderDesc.stages[1] = { "fontstash.frag" };
            ShaderLoadDesc text3DShaderDesc = {};
            text3DShaderDesc.stages[0] = { "fontstash3D.vert" };
            text3DShaderDesc.stages[1] = { "fontstash.frag" };

            addShader(gFontstash.pRenderer, &text2DShaderDesc, &gFontstash.pShaders[0]);
            addShader(gFontstash.pRenderer, &text3DShaderDesc, &gFontstash.pShaders[1]);

            RootSignatureDesc textureRootDesc = { gFontstash.pShaders, 2 };
            addRootSignature(gFontstash.pRenderer, &textureRootDesc, &gFontstash.pRootSignature);
            gFontstash.rootConstantIndex = getDescriptorIndexFromName(gFontstash.pRootSignature, "uRootConstants");
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
        vertexLayout.attribs[1].offset = TinyImageFormat_BitSizeOfBlock((TinyImageFormat)vertexLayout.attribs[0].format) / 8;

        BlendStateDesc blendStateDesc = {};
        blendStateDesc.srcFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.dstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.srcAlphaFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.dstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.colorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateDesc.renderTargetMask = BLEND_STATE_TARGET_ALL;
        blendStateDesc.independentBlend = false;

        DepthStateDesc depthStateDesc[2] = {};
        depthStateDesc[0].depthTest = false;
        depthStateDesc[0].depthWrite = false;

        depthStateDesc[1].depthTest = true;
        depthStateDesc[1].depthWrite = true;
        depthStateDesc[1].depthFunc = (CompareMode)pDesc->depthCompareMode;

        RasterizerStateDesc rasterizerStateDesc[2] = {};
        rasterizerStateDesc[0].cullMode = CULL_MODE_NONE;
        rasterizerStateDesc[0].scissor = true;

        rasterizerStateDesc[1].cullMode = (CullMode)pDesc->cullMode;
        rasterizerStateDesc[1].scissor = true;

        PipelineDesc pipelineDesc = {};
        pipelineDesc.pCache = pDesc->pCache;
        pipelineDesc.type = PIPELINE_TYPE_GRAPHICS;
        pipelineDesc.graphicsDesc.vrFoveatedRendering = true;
        pipelineDesc.graphicsDesc.primitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineDesc.graphicsDesc.renderTargetCount = 1;
        pipelineDesc.graphicsDesc.sampleCount = SAMPLE_COUNT_1;
        pipelineDesc.graphicsDesc.pBlendState = &blendStateDesc;
        pipelineDesc.graphicsDesc.pRootSignature = gFontstash.pRootSignature;
        pipelineDesc.graphicsDesc.pVertexLayout = &vertexLayout;
        pipelineDesc.graphicsDesc.renderTargetCount = 1;
        pipelineDesc.graphicsDesc.sampleCount = SAMPLE_COUNT_1;
        pipelineDesc.graphicsDesc.sampleQuality = 0;
        pipelineDesc.graphicsDesc.pColorFormats = (hz::Format*)&pDesc->colorFormat;

        uint32_t count = pDesc->depthFormat == hz::Format::UNDEFINED ? 1 : 2;
        for (uint32_t i = 0; i < count; ++i)
        {
            pipelineDesc.graphicsDesc.depthStencilFormat = (i > 0) ? pDesc->depthFormat : hz::Format::UNDEFINED;
            pipelineDesc.graphicsDesc.pShaderProgram = gFontstash.pShaders[i];
            pipelineDesc.graphicsDesc.pDepthState = &depthStateDesc[i];
            pipelineDesc.graphicsDesc.pRasterizerState = &rasterizerStateDesc[i];
            addPipeline(gFontstash.pRenderer, &pipelineDesc, &gFontstash.pPipelines[i]);
        }
    }

    if (pDesc->loadType & RELOAD_TYPE_RESIZE)
    {
        gFontstash.scaleBias = { 2.0f / (float)pDesc->width, -2.0f / (float)pDesc->height };
    }

#endif
}

void unloadFontSystem(ReloadType unloadType)
{
#ifdef ENABLE_FORGE_FONTS
    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(gFontstash.pPipelines); ++i)
        {
            if (gFontstash.pPipelines[i])
            {
                removePipeline(gFontstash.pRenderer, gFontstash.pPipelines[i]);
                gFontstash.pPipelines[i] = NULL;
            }
        }

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            removeRootSignature(gFontstash.pRenderer, gFontstash.pRootSignature);

            for (uint32_t i = 0; i < 2; ++i)
            {
                removeShader(gFontstash.pRenderer, gFontstash.pShaders[i]);
            }
        }
    }
#endif
}

void cmdDrawTextWithFont(Cmd* pCmd, float2 screenCoordsInPx, const FontDrawDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.renderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    ASSERT(pDesc);
    ASSERT(pDesc->pText);

    const char* message = pDesc->pText;
    float       x = screenCoordsInPx.getX();
    float       y = screenCoordsInPx.getY();
    int         fontID = pDesc->fontID;
    unsigned    color = pDesc->fontColor;
    float       size = pDesc->fontSize;
    float       spacing = pDesc->fontSpacing;
    float       blur = pDesc->fontBlur;

    FontstashDrawData draw = {};
    draw.text3D = false;
    draw.pCmd = pCmd;
    // clamp the font size to max size.
    // Precomputed font texture puts limitation to the maximum size.
    size = min(size, gFontstash.fontMaxSize);

    FONScontext* fs = gFontstash.pContext;
    fs->params.userPtr = &draw; // -V506 (draw only used inside this function)
    fonsSetSize(fs, size * gFontstash.dpiScaleMin);
    fonsSetFont(fs, fontID);
    fonsSetColor(fs, color);
    fonsSetSpacing(fs, spacing * gFontstash.dpiScaleMin);
    fonsSetBlur(fs, blur);
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    // considering the retina scaling:
    // the render target is already scaled up (w/ retina) and the (x,y) position given to this function
    // is expected to be in the render target's area. Hence, we don't scale up the position again.
    fonsDrawText(fs, x /** gFontstash.dpiScale.x*/, y /** gFontstash.dpiScale.y*/, message, NULL);
#endif
}

void cmdDrawWorldSpaceTextWithFont(Cmd* pCmd, const mat4* pMatWorld, const CameraMatrix* pMatProjView, const FontDrawDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    // ASSERT(pFontStash);
    ASSERT(gFontstash.renderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    ASSERT(pDesc);
    ASSERT(pDesc->pText);
    ASSERT(pMatWorld);
    ASSERT(pMatProjView);

    const char*         message = pDesc->pText;
    const mat4&         worldMat = *pMatWorld;
    const CameraMatrix& projView = *pMatProjView;
    int                 fontID = pDesc->fontID;
    unsigned            color = pDesc->fontColor;
    float               size = pDesc->fontSize;
    float               spacing = pDesc->fontSpacing;
    float               blur = pDesc->fontBlur;

    FontstashDrawData draw = {};
    draw.text3D = true;
    draw.projView = projView;
    draw.worldMat = worldMat;
    draw.pCmd = pCmd;
    // clamp the font size to max size.
    // Precomputed font texture puts limitation to the maximum size.
    size = min(size, gFontstash.fontMaxSize);

    FONScontext* fs = gFontstash.pContext;
    fs->params.userPtr = &draw; // -V506 (draw only used inside this function)
    fonsSetSize(fs, size * gFontstash.dpiScaleMin);
    fonsSetFont(fs, fontID);
    fonsSetColor(fs, color);
    fonsSetSpacing(fs, spacing * gFontstash.dpiScaleMin);
    fonsSetBlur(fs, blur);
    fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
    fonsDrawText(fs, 0.0f, 0.0f, message, NULL);
#endif
}

void cmdDrawDebugFontAtlas(Cmd* pCmd, float2 screenCoordInPx)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.renderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    FontstashDrawData draw = {};
    draw.text3D = false;
    draw.pCmd = pCmd;

    FONScontext* fs = gFontstash.pContext;
    fs->params.userPtr = &draw; // -V506 (draw only used inside this function)
    fonsDrawDebug(fs, screenCoordInPx.x, screenCoordInPx.y);
#endif
}

void fntDefineFonts(const FontDesc* pDescs, uint32_t count, uint32_t* pOutIDs)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(pDescs);
    ASSERT(pOutIDs);
    ASSERT(count > 0);

    arrsetcap(gFontstash.fontBuffers, arrcap(gFontstash.fontBuffers) + count);
    arrsetcap(gFontstash.fontBufferSizes, arrcap(gFontstash.fontBufferSizes) + count);

    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t     id;
        FONScontext* fs = gFontstash.pContext;

        FileStream fh = {};
        if (fsOpenStreamFromPath(RD_FONTS, pDescs[i].pFontPath, FM_READ, &fh))
        {
            ssize_t bytes = fsGetStreamFileSize(&fh);
            void*   buffer = tf_malloc(bytes);
            fsReadFromStream(&fh, buffer, bytes);

            // add buffer to font buffers for cleanup
            arrpush(gFontstash.fontBuffers, buffer);
            ASSERT(bytes < UINT32_MAX);
            arrpush(gFontstash.fontBufferSizes, (uint32_t)bytes);

            fsCloseStream(&fh);

            id = fonsAddFontMem(fs, pDescs[i].pFontName, (unsigned char*)buffer, (int)bytes, 0);
        }
        else
        {
            id = UINT32_MAX;
        }

        ASSERT(id != UINT32_MAX);

        pOutIDs[i] = id;
    }
#endif
}

int2 fntGetFontAtlasSize()
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.renderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    int2         size = {};
    FONScontext* fs = gFontstash.pContext;
    fonsGetAtlasSize(fs, &size.x, &size.y);
    return size;
#endif
}

void fntResetFontAtlas(int2 newAtlasSize)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.renderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    int2 currentSize = fntGetFontAtlasSize();

    if (newAtlasSize.x == 0)
    {
        newAtlasSize.x = currentSize.x;
    }
    if (newAtlasSize.y == 0)
    {
        newAtlasSize.y = currentSize.y;
    }
    FONScontext* fs = gFontstash.pContext;
    fonsResetAtlas(fs, newAtlasSize.x, newAtlasSize.y);
#endif
}

void fntExpandAtlas(int2 additionalSize)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.renderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    FONScontext* fs = gFontstash.pContext;
    fonsExpandAtlas(fs, additionalSize.x, additionalSize.y);
#endif
}

void* fntGetRawFontData(uint32_t fontID)
{
#ifdef ENABLE_FORGE_FONTS
    if (fontID < arrlen(gFontstash.fontBuffers))
        return gFontstash.fontBuffers[fontID];
    else
        return NULL;
#else
    return NULL;
#endif
}

uint32_t fntGetRawFontDataSize(uint32_t fontID)
{
#ifdef ENABLE_FORGE_FONTS
    if (fontID < arrlen(gFontstash.fontBufferSizes))
        return gFontstash.fontBufferSizes[fontID];
    else
        return UINT_MAX;
#else
    return 0;
#endif
}

float2 fntMeasureFontText(const char* pText, const FontDrawDesc* pDrawDesc)
{
#ifdef ENABLE_FORGE_FONTS

    float textBounds[4] = {};

    const int    messageLength = (int)strlen(pText);
    FONScontext* fs = gFontstash.pContext;
    fonsSetSize(fs, pDrawDesc->fontSize * gFontstash.dpiScaleMin);
    fonsSetFont(fs, pDrawDesc->fontID);
    fonsSetColor(fs, pDrawDesc->fontColor);
    fonsSetSpacing(fs, pDrawDesc->fontSpacing * gFontstash.dpiScaleMin);
    fonsSetBlur(fs, pDrawDesc->fontBlur);
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    // considering the retina scaling:
    // the render target is already scaled up (w/ retina) and the (x,y) position given to this function
    // is expected to be in the render target's area. Hence, we don't scale up the position again.
    fonsTextBounds(fs, 0.0f /** gFontstash.dpiScale.x*/, 0.0f /** gFontstash.dpiScale.y*/, pText, pText + messageLength, textBounds);

    return float2(textBounds[2] - textBounds[0], textBounds[3] - textBounds[1]);
#else
    return float2(0, 0);
#endif
}
