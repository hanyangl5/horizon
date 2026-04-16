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

#include "Profiler/IProfiler.h"
//#include "Interfaces/IScreenshot.h"

#if defined(ENABLE_SCREENSHOT)
#include <ThirdParty/tinyimageformat/tinyimageformat_decode.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>
#include "Resources/IResourceLoader.h"
#include "../../RHI/Private/RendererResourceAPI.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "Application/IUI.h"

#include "Core/IMath.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC  tf_malloc
#define STBIW_REALLOC tf_realloc
#define STBIW_FREE    tf_free
#define STBIW_ASSERT  ASSERT
#include <ThirdParty/stb/stb_image_write.h>
static Cmd*               gCmd = 0;
static CmdPool*           pCmdPool = 0;
static Renderer*          pRendererRef = 0;
extern PlatformParameters gPlatformParameters;

bool gCaptureFlag = false;
char gScreenshotName[FS_MAX_PATH] = "";

#ifdef ENABLE_FORGE_REMOTE_UI
extern bool remoteAppIsConnected();
#endif // DEBUG

// Hide or show the UI depending on the current state of gCaptureFlag
void updateUIVisibility()
{
    bool shouldShowUI = !gCaptureFlag;
#ifdef ENABLE_FORGE_REMOTE_UI
    if (!remoteAppIsConnected())
#endif // ENABLE_FORGE_REMOTE_UI
    {
        uiToggleRendering(shouldShowUI);
    }
    toggleProfilerDrawing(shouldShowUI);
}

void initScreenshotInterface(Renderer* pRenderer, Queue* pGraphicsQueue)
{
    ASSERT(pRenderer);
    ASSERT(pGraphicsQueue);
    ASSERTMSG(!pRendererRef, "initScreenshotInterface called but resources were already allocated.");

    pRendererRef = pRenderer;

    // Allocate a command buffer for the GPU work. We use the app's rendering queue to avoid additional sync.
    CmdPoolDesc cmdPoolDesc = {};
    cmdPoolDesc.pQueue = pGraphicsQueue;
    cmdPoolDesc.mTransient = true;
    addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPool);

    CmdDesc cmdDesc = {};
    cmdDesc.pPool = pCmdPool;
#ifdef ENABLE_GRAPHICS_DEBUG
    cmdDesc.pName = "Screenshot Cmd";
#endif // ENABLE_GRAPHICS_DEBUG
    addCmd(pRenderer, &cmdDesc, &gCmd);

    updateUIVisibility();
}

// Helper function to generate screenshot data. Not part of IScreenshot.h
void mapRenderTarget(Renderer* pRenderer, Queue* pQueue, Cmd* pCmd, RenderTarget* pRenderTarget, ResourceState currentResourceState,
                     void* pImageData)
{
    ASSERT(pImageData);
    ASSERT(pRenderTarget);
    ASSERT(pRenderer);

    // Calculate the size of buffer required for copying the src texture.
    D3D12_RESOURCE_DESC                resourceDesc = pRenderTarget->pTexture->mDx.pResource->GetDesc();
    uint64_t                           padded_size = 0;
    uint64_t                           row_size = 0;
    uint32_t                           num_rows = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT imageLayout = {};
    pRenderer->mDx.pDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &imageLayout, &num_rows, &row_size, &padded_size);

    // Add a staging buffer.
    Buffer*    buffer = 0;
    BufferDesc bufferDesc = {};
    bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
    bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
    bufferDesc.mSize = padded_size;
    bufferDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    bufferDesc.mStartState = RESOURCE_STATE_COPY_DEST;
    bufferDesc.mFormat = pRenderTarget->mFormat;
    addBuffer(pRenderer, &bufferDesc, &buffer);

    beginCmd(pCmd);

    // Transition layout to copy data out.
    RenderTargetBarrier srcBarrier = { pRenderTarget, currentResourceState, RESOURCE_STATE_COPY_SOURCE };
    cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

    uint32_t subresource = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    D3D12_TEXTURE_COPY_LOCATION dst = {};

    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = pRenderTarget->pTexture->mDx.pResource;
    src.SubresourceIndex = subresource;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.pResource = buffer->mDx.pResource;
    pCmd->pRenderer->mDx.pDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &dst.PlacedFootprint, nullptr, nullptr, nullptr);

    pCmd->mDx.pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // Transition layout to original state.
    srcBarrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, currentResourceState };
    cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

    endCmd(pCmd);

    // Submit the GPU work.
    QueueSubmitDesc submitDesc = {};
    submitDesc.mCmdCount = 1;
    submitDesc.ppCmds = &pCmd;

    queueSubmit(pQueue, &submitDesc);

    // Wait for work to finish on the GPU.
    waitQueueIdle(pQueue);

    uint8_t* mappedData = (uint8_t*)pImageData;
    uint8_t* srcData = (uint8_t*)buffer->pCpuMappedAddress;
    uint64_t src_row_size = imageLayout.Footprint.RowPitch;

    // Copy row-wise to CPU memory.
    for (uint32_t i = 0; i < num_rows; ++i)
    {
        memcpy(mappedData, srcData, row_size);
        mappedData += row_size;
        srcData += src_row_size;
    }

    removeBuffer(pRenderer, buffer);
}

bool prepareScreenshot(SwapChain* pSwapChain)
{
    ASSERT(pSwapChain);
    return true;
}

struct stbiw_ctx
{
    uint8_t* pBuffer;
    int      mOffset;
};

static void stbiw_func(void* context, void* data, int size)
{
    stbiw_ctx* ctx = (stbiw_ctx*)context;
    memcpy(ctx->pBuffer + ctx->mOffset, data, size);
    ctx->mOffset += size;
}

void captureScreenshot(SwapChain* pSwapChain, uint32_t swapChainRtIndex, bool noAlpha, bool forceFlipRedBlue)
{
    if (!gCaptureFlag)
        return;

    if (!prepareScreenshot(pSwapChain))
        return;

    ASSERT(pRendererRef);
    ASSERT(pSwapChain);
    ASSERT(pCmdPool->pQueue);
    // initScreenshotInterface not called.
    ASSERT(gCmd);

    RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapChainRtIndex];

    // Wait for queue to finish rendering.
    waitQueueIdle(pCmdPool->pQueue);

    // Allocate temp space
    uint16_t byteSize = (uint16_t)TinyImageFormat_BitSizeOfBlock(pRenderTarget->mFormat) / 8;
    uint8_t  channelCount = (uint8_t)TinyImageFormat_ChannelCount(pRenderTarget->mFormat);
    uint32_t size = pRenderTarget->mWidth * pRenderTarget->mHeight * max((uint16_t)4U, byteSize);
    uint8_t* alloc = (uint8_t*)tf_malloc(size);

    resetCmdPool(pRendererRef, pCmdPool);

    // Generate image data buffer.
    mapRenderTarget(pRendererRef, pCmdPool->pQueue, gCmd, pRenderTarget, RESOURCE_STATE_PRESENT, alloc);

    char screenshotFileName[FS_MAX_PATH] = {};
    strcat(screenshotFileName, gScreenshotName);
    strcat(screenshotFileName, (COLOR_SPACE_SDR_SRGB < pSwapChain->mColorSpace) ? ".hdr" : ".png");
    void* pEncoded = nullptr;
    int   encodedSize = 0;

    if (COLOR_SPACE_SDR_SRGB < pSwapChain->mColorSpace)
    {
        // decode pixels
        ASSERT(TinyImageFormat_CanDecodeLogicalPixelsF(pRenderTarget->mFormat));
        TinyImageFormat_FetchInput fetchInput = { { (void*)alloc } };
        uint32_t                   floatBufferSize = pRenderTarget->mWidth * pRenderTarget->mHeight * sizeof(float4);
        float*                     pDecoded = (float*)tf_malloc(floatBufferSize);
        const bool                 result = TinyImageFormat_DecodeLogicalPixelsF(pRenderTarget->mFormat, &fetchInput,
                                                                 pRenderTarget->mWidth * pRenderTarget->mHeight, pDecoded);
        ASSERT(result);

        pEncoded = tf_malloc(floatBufferSize);
        stbiw_ctx ctx = { (uint8_t*)pEncoded };
        stbi_write_hdr_to_func(stbiw_func, &ctx, pRenderTarget->mWidth, pRenderTarget->mHeight, 4, pDecoded);
        tf_free(pDecoded);
        encodedSize = ctx.mOffset;
    }
    else
    {
        // Flip the BGRA to RGBA
        const bool flipRedBlueChannel = forceFlipRedBlue || !(pRenderTarget->mFormat == TinyImageFormat_R8G8B8A8_UNORM ||
                                                              pRenderTarget->mFormat == TinyImageFormat_R8G8B8A8_SRGB);

        if (flipRedBlueChannel)
        {
            int8_t* imageData = ((int8_t*)alloc);

            for (uint32_t h = 0; h < pRenderTarget->mHeight; ++h)
            {
                for (uint32_t w = 0; w < pRenderTarget->mWidth; ++w)
                {
                    uint32_t pixelIndex = (h * pRenderTarget->mWidth + w) * channelCount;
                    int8_t*  pixel = imageData + pixelIndex;

                    // Swap blue and red.
                    int8_t r = pixel[0];
                    pixel[0] = pixel[2];
                    pixel[2] = r;
                }
            }
        }

        if (noAlpha)
        {
            uint8_t* imageData = ((uint8_t*)alloc);

            for (uint32_t i = 0; i < pRenderTarget->mWidth * pRenderTarget->mHeight; i++)
            {
                void* dst = &imageData[i * 3]; // RGB
                void* src = &imageData[i * 4]; // RGBA
                memmove(dst, src, byteSize);
            }

            byteSize = byteSize - 1;
            channelCount = channelCount - 1;
        }

        // Convert image data to png. Use global stbi_write_png_compression_level to configure compression level
        stbi_write_png_compression_level = 4; // Default is 8, which takes longer to process
        pEncoded = stbi_write_png_to_mem((unsigned char*)alloc, pRenderTarget->mWidth * byteSize, pRenderTarget->mWidth,
                                         pRenderTarget->mHeight, channelCount, &encodedSize);
    }

    // Save screenshot to disk.
    FileStream fs = {};
    if (fsOpenStreamFromPath(RD_SCREENSHOTS, screenshotFileName, FM_WRITE, &fs))
    {
        LOGF(eINFO, "Writing screenshot to %s", screenshotFileName);
        VERIFY(fsWriteToStream(&fs, pEncoded, (size_t)encodedSize) == (size_t)encodedSize);
        VERIFY(fsCloseStream(&fs));
    }
    else
    {
        LOGF(eERROR, "Failed to open file for a screenshot: %s", gScreenshotName);
    }

    tf_free(alloc);
    tf_free(pEncoded);

    gCaptureFlag = false;
    updateUIVisibility();
}

void exitScreenshotInterface()
{
    if (gCmd != nullptr)
    {
        removeCmd(pRendererRef, gCmd);
        gCmd = nullptr;
    }
    if (pRendererRef != nullptr)
    {
        removeCmdPool(pRendererRef, pCmdPool);
        pCmdPool = nullptr;
    }
    pRendererRef = nullptr;
}

void setCaptureScreenshot(const char* name)
{
    gCaptureFlag = true;
    ASSERT(strlen(name) < sizeof(gScreenshotName));
    strncpy(gScreenshotName, name, sizeof(gScreenshotName) - 1);
    gScreenshotName[sizeof(gScreenshotName) - 1] = 0;
    updateUIVisibility();
}

#else
void initScreenshotInterface(Renderer* pRenderer, Queue* pGraphicsQueue) {}
bool prepareScreenshot() { return false; }
void captureScreenshot(SwapChain* pSwapChain, uint32_t swapChainRtIndex, bool noAlpha, bool forceFlipRedBlue) {}
void exitScreenshotInterface() {}
void setCaptureScreenshot(const char* name) {}
#endif
