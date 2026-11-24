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

#pragma once

#include <ThirdParty/tinydds/tinydds.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_apis.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_base.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_bits.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>
#include <ThirdParty/tinyktx/tinyktx.h>

#include "RHI/IGraphics.h"
#include "Platform/IOperatingSystem.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"

/************************************************************************/
// Surface Utils
/************************************************************************/
static inline bool util_get_surface_info(uint32_t Width, uint32_t Height, hz::Format fmt, uint32_t* outNumBytes, uint32_t* outRowBytes,
                                         uint32_t* outNumRows)
{
    uint64_t numBytes = 0;
    uint64_t rowBytes = 0;
    uint64_t numRows = 0;

    uint32_t bpp = TinyImageFormat_BitSizeOfBlock((TinyImageFormat)fmt);
    bool     compressed = TinyImageFormat_IsCompressed((TinyImageFormat)fmt);
    bool     planar = TinyImageFormat_IsPlanar((TinyImageFormat)fmt);
    // #TODO
    bool     packed = false;

    if (compressed)
    {
        uint32_t blockWidth = TinyImageFormat_WidthOfBlock((TinyImageFormat)fmt);
        uint32_t blockHeight = TinyImageFormat_HeightOfBlock((TinyImageFormat)fmt);
        uint32_t numBlocksWide = 0;
        uint32_t numBlocksHigh = 0;
        if (Width > 0)
        {
            numBlocksWide = max(1U, (Width + (blockWidth - 1)) / blockWidth);
        }
        if (Height > 0)
        {
            numBlocksHigh = max(1u, (Height + (blockHeight - 1)) / blockHeight);
        }

        rowBytes = numBlocksWide * (bpp >> 3);
        numRows = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed) //-V547
    {
        LOGF(Error, "Not implemented");
        return false;
        // rowBytes = ((uint64_t(Width) + 1u) >> 1) * bpe;
        // numRows = uint64_t(Height);
        // numBytes = rowBytes * Height;
    }
    else if (planar)
    {
        uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes((TinyImageFormat)fmt);

        for (uint32_t i = 0; i < numOfPlanes; ++i)
        {
            numBytes += TinyImageFormat_PlaneWidth((TinyImageFormat)fmt, i, Width) *
                        TinyImageFormat_PlaneHeight((TinyImageFormat)fmt, i, Height) *
                        TinyImageFormat_PlaneSizeOfBlock((TinyImageFormat)fmt, i);
        }

        numRows = 1;
        rowBytes = numBytes;
    }
    else
    {
        if (!bpp)
            return false;

        rowBytes = ((uint64_t)Width * bpp + 7u) / 8u; // round up to nearest byte
        numRows = (uint64_t)Height;
        numBytes = rowBytes * Height;
    }

    if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX) //-V560
        return false;

    if (outNumBytes)
    {
        *outNumBytes = (uint32_t)numBytes;
    }
    if (outRowBytes)
    {
        *outRowBytes = (uint32_t)rowBytes;
    }
    if (outNumRows)
    {
        *outNumRows = (uint32_t)numRows;
    }

    return true;
}

static inline uint32_t util_get_surface_size(hz::Format Format, uint32_t Width, uint32_t Height, uint32_t Depth, uint32_t rowStride,
                                             uint32_t sliceStride, uint32_t baseMipLevel, uint32_t MipLevels, uint32_t baseArrayLayer,
                                             uint32_t arrayLayers)
{
    uint32_t requiredSize = 0;
    for (uint32_t s = baseArrayLayer; s < baseArrayLayer + arrayLayers; ++s)
    {
        uint32_t w = Width;
        uint32_t h = Height;
        uint32_t d = Depth;

        for (uint32_t m = baseMipLevel; m < baseMipLevel + MipLevels; ++m)
        {
            uint32_t rowBytes = 0;
            uint32_t numRows = 0;

            if (!util_get_surface_info(w, h, Format, NULL, &rowBytes, &numRows))
            {
                return 0;
            }

            requiredSize += round_up(d * round_up(rowBytes, rowStride) * numRows, sliceStride);

            w = w >> 1;
            h = h >> 1;
            d = d >> 1;
            if (w == 0)
            {
                w = 1;
            }
            if (h == 0)
            {
                h = 1;
            }
            if (d == 0)
            {
                d = 1;
            }
        }
    }

    return requiredSize;
}

#define RETURN_IF_FAILED(exp) \
    if (!(exp))               \
    {                         \
        return false;         \
    }

/************************************************************************/
// DDS Loading
/************************************************************************/
// BC4/BC5 channels use the same endpoint interpolation as a DXT5 alpha block.
inline uint8_t decodeBC5SingleTexelChannel(const uint8_t* block)
{
    const uint32_t index = block[2] & 7u;
    if (index < 2)
        return block[index];
    if (block[0] > block[1])
        return (uint8_t)(((8u - index) * block[0] + (index - 1u) * block[1]) / 7u);
    if (index >= 6)
        return index == 6 ? 0 : 255;
    return (uint8_t)(((6u - index) * block[0] + (index - 1u) * block[1]) / 5u);
}

inline bool loadDDSTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
    RETURN_IF_FAILED(pStream);

    ssize_t ddsDataSize = GetStreamFileSize(pStream);
    RETURN_IF_FAILED(ddsDataSize <= UINT32_MAX);

    TinyDDS_Callbacks callbacks{ [](void* user, char const* msg)
                                 {
                                     UNREF_PARAM(user);
                                     LOGF(Error, "%s", msg);
                                 },
                                 [](void* user, size_t Size)
                                 {
                                     UNREF_PARAM(user);
                                     return MemAllocate(Size);
                                 },
                                 [](void* user, void* memory)
                                 {
                                     UNREF_PARAM(user);
                                     MemDeallocate(memory);
                                 },
                                 [](void* user, void* buffer, size_t byteCount)
                                 { return ReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
                                 [](void* user, int64_t offset)
                                 { return SeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
                                 [](void* user) { return (int64_t)GetStreamPosition((FileStream*)user); } };

    TinyDDS_ContextHandle ctx = TinyDDS_CreateContext(&callbacks, (void*)pStream);
    bool                  headerOkay = TinyDDS_ReadHeader(ctx);
    if (!headerOkay)
    {
        TinyDDS_DestroyContext(ctx);
        return false;
    }

    TextureDesc& textureDesc = *pOutDesc;
    textureDesc.Width = TinyDDS_Width(ctx);
    textureDesc.Height = TinyDDS_Height(ctx);
    textureDesc.Depth = max(1U, TinyDDS_Depth(ctx));
    textureDesc.ArraySize = max(1U, TinyDDS_ArraySlices(ctx));
    textureDesc.MipLevels = max(1U, TinyDDS_NumberOfMipmaps(ctx));
    textureDesc.Format = (hz::Format)TinyImageFormat_FromTinyDDSFormat(TinyDDS_GetFormat(ctx));
    textureDesc.Descriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.SampleCount = SAMPLE_COUNT_1;

    if (textureDesc.Format == hz::Format::UNDEFINED)
    {
        TinyDDS_DestroyContext(ctx);
        return false;
    }

    if (TinyDDS_IsCubemap(ctx))
    {
        textureDesc.ArraySize *= 6;
        textureDesc.Descriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
    }

    TinyDDS_DestroyContext(ctx);

    return true;
}
/************************************************************************/
// KTX Loading
/************************************************************************/
inline bool loadKTXTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
    RETURN_IF_FAILED(pStream);

    ssize_t ktxDataSize = GetStreamFileSize(pStream);
    RETURN_IF_FAILED(ktxDataSize <= UINT32_MAX);

    TinyKtx_Callbacks callbacks{ [](void* user, char const* msg)
                                 {
                                     UNREF_PARAM(user);
                                     LOGF(Error, "%s", msg);
                                 },
                                 [](void* user, size_t Size)
                                 {
                                     UNREF_PARAM(user);
                                     return MemAllocate(Size);
                                 },
                                 [](void* user, void* memory)
                                 {
                                     UNREF_PARAM(user);
                                     MemDeallocate(memory);
                                 },
                                 [](void* user, void* buffer, size_t byteCount)
                                 { return ReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
                                 [](void* user, int64_t offset)
                                 { return SeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
                                 [](void* user) { return (int64_t)GetStreamPosition((FileStream*)user); } };

    TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, pStream);
    bool                  headerOkay = TinyKtx_ReadHeader(ctx);
    if (!headerOkay)
    {
        TinyKtx_DestroyContext(ctx);
        return false;
    }

    TextureDesc& textureDesc = *pOutDesc;
    textureDesc.Width = TinyKtx_Width(ctx);
    textureDesc.Height = TinyKtx_Height(ctx);
    textureDesc.Depth = max(1U, TinyKtx_Depth(ctx));
    textureDesc.ArraySize = max(1U, TinyKtx_ArraySlices(ctx));
    textureDesc.MipLevels = max(1U, TinyKtx_NumberOfMipmaps(ctx));
    textureDesc.Format = (hz::Format)TinyImageFormat_FromTinyKtxFormat(TinyKtx_GetFormat(ctx));
    textureDesc.Descriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.SampleCount = SAMPLE_COUNT_1;

    if (textureDesc.Format == hz::Format::UNDEFINED)
    {
        TinyKtx_DestroyContext(ctx);
        return false;
    }

    if (TinyKtx_IsCubemap(ctx))
    {
        textureDesc.ArraySize *= 6;
        textureDesc.Descriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
    }

    TinyKtx_DestroyContext(ctx);

    return true;
}
