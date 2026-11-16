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

#include "../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../Utilities/Interfaces/IFileSystem.h"
#include "../../../Utilities/Interfaces/ILog.h"
#include "../../../Utilities/Interfaces/IToolFileSystem.h"

#include "AssetPipeline.h"

// Math
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../../Utilities/ThirdParty/OpenSource/ModifiedSonyMath/vectormath.hpp"

#include "../../../Resources/ResourceLoader/TextureContainers.h"

// TinyImage
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinydds/tinydds.h"
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyktx/tinyktx.h"

// ISPC texcomp
#include "../../ThirdParty/OpenSource/ISPCTextureCompressor/ispc_texcomp/ispc_texcomp.h"

// Nothings
#define STBI_NO_STDIO
#define STBI_ASSERT(x)         ASSERT(x)
#define STBI_MALLOC(sz)        MemAllocate(sz)
#define STBI_REALLOC(p, newsz) MemReallocate(p, newsz)
#define STBI_FREE(p)           MemDeallocate(p)
#define STB_IMAGE_IMPLEMENTATION
#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_image_resize.h"

#include "../../../Utilities/Interfaces/IMemory.h" //NOTE: this should be the last include in a .cpp

#define IS_POWER_OF_TWO(x) ((x) != 0 && ((x) & ((x)-1)) == 0)

const char* gExtensions[] = { "dds", "ktx"
#ifdef PROSPERO_GNF
                              ,
                              "gnf", "gnf"
#endif
#ifdef XBOX_SCARLETT_DDS
                              ,
                              "dds"
#endif
};

TinyDDS_WriteCallbacks ddsWriteCallbacks{ [](void* user, char const* msg)
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
                                          [](void* user, const void* buffer, size_t byteCount)
                                          { WriteToStream((FileStream*)user, buffer, (ssize_t)byteCount); } };

TinyDDS_Callbacks ddsReadCallbacks{ [](void* user, char const* msg)
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

TinyKtx_WriteCallbacks ktxWriteCallbacks{ [](void* user, char const* msg)
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
                                          [](void* user, const void* buffer, size_t byteCount)
                                          { WriteToStream((FileStream*)user, buffer, (ssize_t)byteCount); } };

TinyKtx_Callbacks ktxReadCallbacks{ [](void* user, char const* msg)
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

typedef struct InputTextureData
{
    TextureDesc mDesc;
    uint8_t*    Data[MAX_MIPLEVELS];
    uint32_t    mDataSize[MAX_MIPLEVELS];
    bool        isCompressed;
} InputTextureData;

typedef struct CompressImageDescriptor
{
    TextureCompression mCompression;
    ASTC               mASTCCompression;
    DXT                mDXTCompression;
} CompressImageDescriptor;

uint8_t* ResizeImage(uint8_t* ppData, const uint32_t Width, const uint32_t Height, const uint32_t newWidth, const uint32_t newHeight,
                     const uint32_t components, const uint32_t mip)
{
    uint8_t* pResizedImageData = (uint8_t*)MemAllocate(newWidth * newHeight * components);
    // stbir_resize_uint8(ppData, Width, Height, Width * components, pResizedImageData, newWidth, newHeight, newWidth * components,
    // components);
    stbir_resize_uint8_srgb_edgemode(ppData, Width, Height, Width * components, pResizedImageData, newWidth, newHeight,
                                     newWidth * components, components, 1, 0, stbir_edge::STBIR_EDGE_CLAMP);
    LOGF(Info, "Resized image %ux%u -> %ux%u (%ux%u)", Width, Height, newWidth, newHeight, newWidth << mip, newHeight << mip);
    return pResizedImageData;
}

hz::Format GetASTCFormat(ASTC astc, bool isSrgb)
{
    hz::Format outFormat = hz::Format::UNDEFINED;
    switch (astc)
    {
    case ASTC_4x4:
    case ASTC_4x4_SLOW:
        outFormat = isSrgb ? hz::Format::ASTC_4x4_SRGB : hz::Format::ASTC_4x4_UNORM;
        break;
    case ASTC_8x8:
    case ASTC_8x8_SLOW:
        outFormat = isSrgb ? hz::Format::ASTC_8x8_SRGB : hz::Format::ASTC_8x8_UNORM;
        break;
    default:
        LOGF(Error, "Unknown ASTC compression!");
    }
    return outFormat;
}

hz::Format GetBCFormat(DXT dxt, uint32_t channels, bool isSrgb, bool isSigned)
{
    hz::Format outFormat = hz::Format::UNDEFINED;
    switch (dxt)
    {
    case DXT_BC1:
        outFormat = isSrgb ? (channels < 4 ? hz::Format::DXBC1_RGB_SRGB : hz::Format::DXBC1_RGBA_SRGB)
                           : (channels < 4 ? hz::Format::DXBC1_RGB_UNORM : hz::Format::DXBC1_RGBA_UNORM);
        break;
    case DXT_BC3:
        outFormat = isSrgb ? hz::Format::DXBC3_SRGB : hz::Format::DXBC3_UNORM;
        break;
    case DXT_BC4:
        outFormat = isSigned ? hz::Format::DXBC4_SNORM : hz::Format::DXBC4_UNORM;
        break;
    case DXT_BC5:
        outFormat = isSigned ? hz::Format::DXBC5_SNORM : hz::Format::DXBC5_UNORM;
        break;
    case DXT_BC6:
        outFormat = isSigned ? hz::Format::DXBC6H_SFLOAT : hz::Format::DXBC6H_UFLOAT;
        break;
    case DXT_BC7:
        outFormat = isSrgb ? hz::Format::DXBC7_SRGB : hz::Format::DXBC7_UNORM;
        break;
    default:
        LOGF(Error, "Unknown DXT compression!");
    }

    return outFormat;
}

hz::Format GetOutputTextureFormat(ProcessTexturesParams* pTexturesParams, TextureDesc* pDesc,
                                  CompressImageDescriptor* pOutCompressImageDescriptor)
{
    const uint32_t channels = TinyImageFormat_ChannelCount((TinyImageFormat)pDesc->Format);
    hz::Format     outFormat = hz::Format::UNDEFINED;
    bool           isSigned = TinyImageFormat_IsSigned((TinyImageFormat)pDesc->Format);
    bool           isSrgb = TinyImageFormat_IsSRGB((TinyImageFormat)pDesc->Format);

    ASTC astcCompression = pTexturesParams->mOverrideASTC != ASTC_NONE ? pTexturesParams->mOverrideASTC : ASTC_4x4; // Default to ASTC_4x4
    DXT  dxtCompression = DXT_NONE;

    if (pTexturesParams->mOverrideBC == DXT_NONE)
    {
        if (channels == 1)
        {
            dxtCompression = DXT_BC4;
        }
        else if (channels == 2)
        {
            dxtCompression = DXT_BC5;
        }
        else if (channels == 3)
        {
            dxtCompression = DXT_BC1;
        }
        else if (channels == 4)
        {
            dxtCompression = DXT_BC3;
        }
    }
    else
    {
        dxtCompression = pTexturesParams->mOverrideBC;
    }

    pOutCompressImageDescriptor->mCompression = pTexturesParams->mCompression;
    pOutCompressImageDescriptor->mASTCCompression = astcCompression;
    pOutCompressImageDescriptor->mDXTCompression = dxtCompression;

    switch (pTexturesParams->mCompression)
    {
    case COMPRESSION_ASTC:
        outFormat = GetASTCFormat(astcCompression, isSrgb);
        break;
    case COMPRESSION_BC:
        outFormat = GetBCFormat(dxtCompression, channels, isSrgb, isSigned);
        break;
    default:
        outFormat = pDesc->Format;
        break;
    }

    return outFormat;
}

bool LoadTextureData(ResourceDirectory resourceDir, const char* pFilepath, const char* pExtension, ProcessTexturesParams* pTextureParams,
                     InputTextureData* pOut)
{
    ASSERT(pFilepath);
    ASSERT(pOut);
    ASSERT(pExtension);
    ASSERT(pTextureParams);

    bool success = true;

    // Read input file
    FileStream file = {};
    if (!OpenStreamFromPath(resourceDir, pFilepath, FileMode(FileModeRead | FileModeAllowRead), &file))
    {
        LOGF(Error, "Could not open file '%s'.", pFilepath);
        success = false;
        return success;
    }

    ssize_t fileSize = GetStreamFileSize(&file);
    if (fileSize > INT32_MAX)
    {
        LOGF(Warning, "File '%s' exceeds max handled filesize, size: %zi max: %i", pFilepath, fileSize, INT32_MAX);
        success = false;
    }

    if (STRCMP(pExtension, gExtensions[CONTAINER_KTX]))
    {
        TinyKtx_ContextHandle tinyKTXCtx = TinyKtx_CreateContext(&ktxReadCallbacks, (void*)&file);
        bool                  headerOkay = TinyKtx_ReadHeader(tinyKTXCtx);
        if (!headerOkay)
        {
            TinyKtx_DestroyContext(tinyKTXCtx);
            success = false;
            return success;
        }

        pOut->mDesc.Width = TinyKtx_Width(tinyKTXCtx);
        pOut->mDesc.Height = TinyKtx_Height(tinyKTXCtx);
        pOut->mDesc.Depth = max(1U, TinyKtx_Depth(tinyKTXCtx));
        pOut->mDesc.ArraySize = max(1U, TinyKtx_ArraySlices(tinyKTXCtx));
        pOut->mDesc.MipLevels = max(1U, TinyKtx_NumberOfMipmaps(tinyKTXCtx));
        pOut->mDesc.Format = (hz::Format)TinyImageFormat_FromTinyKtxFormat(TinyKtx_GetFormat(tinyKTXCtx));
        pOut->mDesc.Descriptors = DescriptorTypeTexture;
        pOut->mDesc.SampleCount = SAMPLE_COUNT_1;

        if (pOut->mDesc.Format == hz::Format::UNDEFINED)
        {
            TinyKtx_DestroyContext(tinyKTXCtx);
            success = false;
        }

        if (TinyKtx_IsCubemap(tinyKTXCtx))
        {
            pOut->mDesc.ArraySize *= 6;
            pOut->mDesc.Descriptors |= DescriptorTypeTextureCube;
        }

        pOut->isCompressed = TinyImageFormat_IsCompressed((TinyImageFormat)pOut->mDesc.Format);

        for (uint32_t mip = 0; mip < pOut->mDesc.MipLevels; ++mip)
        {
            pOut->mDataSize[mip] = TinyKtx_ImageSize(tinyKTXCtx, mip);
            pOut->Data[mip] = (uint8_t*)MemAllocate(pOut->mDataSize[mip]);
            memcpy(pOut->Data[mip], (uint8_t*)TinyKtx_ImageRawData(tinyKTXCtx, mip), pOut->mDataSize[mip]);
        }

        TinyKtx_DestroyContext(tinyKTXCtx);
    }
    else if (STRCMP(pExtension, gExtensions[CONTAINER_DDS]))
    {
        TinyDDS_ContextHandle tinyDDSCtx = TinyDDS_CreateContext(&ddsReadCallbacks, (void*)&file);
        bool                  headerOkay = TinyDDS_ReadHeader(tinyDDSCtx);
        if (!headerOkay)
        {
            TinyDDS_DestroyContext(tinyDDSCtx);
            LOGF(Error, "Could not Load dds texture information '%s'.", pFilepath);
            success = false;
        }

        pOut->mDesc.Width = TinyDDS_Width(tinyDDSCtx);
        pOut->mDesc.Height = TinyDDS_Height(tinyDDSCtx);
        pOut->mDesc.Depth = max(1U, TinyDDS_Depth(tinyDDSCtx));
        pOut->mDesc.ArraySize = max(1U, TinyDDS_ArraySlices(tinyDDSCtx));
        pOut->mDesc.MipLevels = max(1U, TinyDDS_NumberOfMipmaps(tinyDDSCtx));
        pOut->mDesc.Format = (hz::Format)TinyImageFormat_FromTinyDDSFormat(TinyDDS_GetFormat(tinyDDSCtx));
        pOut->mDesc.Descriptors = DescriptorTypeTexture;
        pOut->mDesc.SampleCount = SAMPLE_COUNT_1;

        if (pOut->mDesc.Format == hz::Format::UNDEFINED)
        {
            TinyDDS_DestroyContext(tinyDDSCtx);
            success = false;
        }

        if (TinyDDS_IsCubemap(tinyDDSCtx))
        {
            pOut->mDesc.ArraySize *= 6;
            pOut->mDesc.Descriptors |= DescriptorTypeTextureCube;
        }

        pOut->isCompressed = TinyImageFormat_IsCompressed((TinyImageFormat)pOut->mDesc.Format);

        for (uint32_t mip = 0; mip < pOut->mDesc.MipLevels; ++mip)
        {
            pOut->mDataSize[mip] = TinyDDS_ImageSize(tinyDDSCtx, mip);
            pOut->Data[mip] = (uint8_t*)MemAllocate(pOut->mDataSize[mip]);
            memcpy(pOut->Data[mip], (uint8_t*)TinyDDS_ImageRawData(tinyDDSCtx, mip), pOut->mDataSize[mip]);
        }

        TinyDDS_DestroyContext(tinyDDSCtx);
    }
    else
    {
        uint8_t* pFileData = (uint8_t*)MemAllocate(fileSize);
        ssize_t  fileReadBytes = ReadFromStream(&file, pFileData, fileSize);

        if (fileSize != fileReadBytes)
        {
            LOGF(Error, "Could not read all bytes from file '%s' %zi/%zi", pFilepath, fileReadBytes, fileSize);
            success = false;
        }

        int32_t componentCount = 0;
        int32_t forceComponents = 0; // Optional to foce loading a certain amount of channels

        int32_t imageWidth = 0;
        int32_t imageHeight = 0;
        int32_t imageDepth = 1; // TODO: support loading 3D images stbi_image seems not to support this?
        stbi_info_from_memory(pFileData, (int32_t)fileSize, &imageWidth, &imageHeight, &componentCount);

        hz::Format textureFormat = hz::Format::UNDEFINED;
        if (pTextureParams->mInputLinearColorSpace)
        {
            // Linear Color Space
            if (pTextureParams->mCompression == TextureCompression::COMPRESSION_ASTC)
            {
                // ISPC Texture Compressor expects 32bit/pixel for ASTC compression
                forceComponents = 4;
                textureFormat = hz::Format::R8G8B8A8_UNORM;
            }
            else
            {
                // BC compression (or uncompressed) doesn't need to have forced channels.
                switch (componentCount)
                {
                case 1:
                    textureFormat = hz::Format::R8_UNORM;
                    break;
                case 2:
                    textureFormat = hz::Format::R8G8_UNORM;
                    break;
                case 3:
                    textureFormat = hz::Format::R8G8B8_UNORM;
                    break;
                case 4:
                    textureFormat = hz::Format::R8G8B8A8_UNORM;
                    break;
                default:
                    textureFormat = hz::Format::UNDEFINED;
                    break;
                }

                // Unless we need to do BC1 compression; ISPC Texture Compressor expects 32bit/pixel for BC1
                if (pTextureParams->mOverrideBC == DXT_BC1 ||
                    (pTextureParams->mCompression == TextureCompression::COMPRESSION_BC && componentCount == 3))
                {
                    forceComponents = 4;
                    textureFormat = hz::Format::R8G8B8A8_UNORM;
                    pTextureParams->mOverrideBC = DXT_BC1;
                }
            }
        }
        else
        {
            // sRGB Color Space
            // DDS container srgb only supports 4 components
            // KTX container with srgb doesn't support 3 components (at least gives some issues on Android)
            forceComponents = 4;
            textureFormat = hz::Format::R8G8B8A8_SRGB;
        }

        if (textureFormat == hz::Format::UNDEFINED)
        {
            LOGF(LogLevel::Error, "Cannot process texture with texure format UNDEFINED");
            success = false;
        }

        if (pFileData)
        {
            bool isHDR = stbi_is_hdr_from_memory(pFileData, (int32_t)fileSize);
            if (isHDR)
            {
                LOGF(LogLevel::Info, "HDR file being loaded is being mapped to LDR with gamma : %f and scale factor: %f", stbi__l2h_gamma,
                     stbi__l2h_scale);
            }
        }

        if (success)
        {
            pOut->Data[0] =
                stbi_load_from_memory(pFileData, (int32_t)fileSize, &imageWidth, &imageHeight, &componentCount, forceComponents);
            pOut->mDataSize[0] = imageWidth * imageHeight * max(forceComponents, componentCount);

            pOut->mDesc.Width = imageWidth;
            pOut->mDesc.Height = imageHeight;
            pOut->mDesc.Depth = imageDepth;

            pOut->mDesc.MipLevels = 1;
            pOut->mDesc.ArraySize = 1;
            pOut->mDesc.Descriptors = DescriptorTypeTexture;
            pOut->mDesc.Format = textureFormat;
        }

        // Release loaded input file data
        if (pFileData)
        {
            MemDeallocate(pFileData);
        }
    }

    CloseStream(&file);

    return success;
}

bool ASTCCompression(uint8_t* ppData[MAX_MIPLEVELS], uint8_t* ppOutCompressed[MAX_MIPLEVELS], uint32_t* pCompressedSize,
                     CompressImageDescriptor* pDesc, TextureDesc* pTexDesc)
{
    ASSERT(ppData[0]);
    ASSERT(pDesc);
    ASSERT(pTexDesc->Width && pTexDesc->Height); // widht/Height cannot be 0

    const uint32_t blockSizeX = pDesc->mASTCCompression == ASTC_4x4 || pDesc->mASTCCompression == ASTC_4x4_SLOW ? 4 : 8;
    const uint32_t blockSizeY = pDesc->mASTCCompression == ASTC_4x4 || pDesc->mASTCCompression == ASTC_4x4_SLOW ? 4 : 8;
    const uint32_t channels = TinyImageFormat_ChannelCount((TinyImageFormat)pTexDesc->Format);
    ASSERT(channels >= 3); // ISPC astc compression requires atleast 3 channels

    if (TinyImageFormat_BitSizeOfBlock((TinyImageFormat)pTexDesc->Format) != 32)
    {
        LOGF(LogLevel::Error, "Fast ISPC Texture Compressor only supports 32bits per pixel for ASTC");
        return false;
    }

    // Get astc encoder settings
    astc_enc_settings astcEncSettings = {};
    if (channels > 3)
    {
        if (pDesc->mASTCCompression == ASTC_4x4_SLOW || pDesc->mASTCCompression == ASTC_8x8_SLOW)
        {
            GetProfile_astc_alpha_slow(&astcEncSettings, blockSizeX, blockSizeY);
        }
        else
        {
            GetProfile_astc_alpha_fast(&astcEncSettings, blockSizeX, blockSizeY);
        }
    }
    else
    {
        GetProfile_astc_fast(&astcEncSettings, blockSizeX, blockSizeY);
    }

    // Store if texture mip 0 is padded and use that in the and for the texture descriptor
    uint32_t adjustedWidth = pTexDesc->Width;
    uint32_t adjustedHeight = pTexDesc->Height;
    uint32_t slices = pTexDesc->ArraySize;

    for (uint32_t i = 0; i < pTexDesc->MipLevels; ++i)
    {
        uint32_t Width = max(1u, (pTexDesc->Width >> i));
        uint32_t Height = max(1u, (pTexDesc->Height >> i));
        uint8_t* Data = NULL;
        for (uint32_t slice_index = 0; slice_index < slices; ++slice_index)
        {
            bool     padded = false;
            uint32_t slice_offset = Width * Height * channels * slice_index;

            if (Width % blockSizeX != 0 || Height % blockSizeY != 0)
            {
                rgba_surface input;
                input.Width = Width;
                input.Height = Height;
                input.stride = Width * channels;
                input.ptr = ppData[i] + slice_offset;

                uint32_t resizedImageWidth = Width % blockSizeX == 0 ? Width : Width + (blockSizeX - Width % blockSizeX);
                uint32_t resizedImageHeight = Height % blockSizeY == 0 ? Height : Height + (blockSizeY - Height % blockSizeY);

                Data = (uint8_t*)MemAllocate(resizedImageWidth * resizedImageHeight * channels);
                rgba_surface input_padded;
                input_padded.Width = resizedImageWidth;
                input_padded.Height = resizedImageHeight;
                input_padded.stride = resizedImageWidth * channels;
                input_padded.ptr = Data;

                ReplicateBorders(&input_padded, &input, 0, 0, 32 /* or 64 */);

                Width = resizedImageWidth;
                Height = resizedImageHeight;
                padded = true;

                if (i == 0)
                {
                    adjustedWidth = Width;
                    adjustedHeight = Height;
                }
            }
            else
            {
                Data = ppData[i] + slice_offset;
            }

            const uint32_t xblocks = (Width + blockSizeX - 1) / blockSizeX;
            const uint32_t yblocks = (Height + blockSizeY - 1) / blockSizeY;
            const uint32_t bytesPerBlock = 16;

            uint32_t compressed_offset = 0;
            if (ppOutCompressed[i] == NULL)
            {
                pCompressedSize[i] = xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)MemAllocate(pCompressedSize[i]);
            }
            else
            {
                compressed_offset = pCompressedSize[i];
                pCompressedSize[i] += xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)MemReallocate(ppOutCompressed[i], pCompressedSize[i]);
            }

            rgba_surface input;
            input.Width = Width;
            input.Height = Height;
            input.stride = Width * channels;
            input.ptr = Data;

            CompressBlocksASTC(&input, ppOutCompressed[i] + compressed_offset, &astcEncSettings);

            if (padded)
            {
                MemDeallocate(Data);
            }
        }
    }

    // Set image Size to padding Size
    pTexDesc->Width = adjustedWidth;
    pTexDesc->Height = adjustedHeight;

    return true;
}

typedef void (*BCCompressionFunc)(const rgba_surface* src, uint8_t* dst);

#define DECLARE_COMPRESS_FUNCTION_BC6H(profile)                              \
    void CompressBlocksBC6H_##profile(const rgba_surface* src, uint8_t* dst) \
    {                                                                        \
        bc6h_enc_settings settings;                                          \
        GetProfile_bc6h_##profile(&settings);                                \
        CompressBlocksBC6H(src, dst, &settings);                             \
    }

DECLARE_COMPRESS_FUNCTION_BC6H(veryfast);
DECLARE_COMPRESS_FUNCTION_BC6H(fast);
DECLARE_COMPRESS_FUNCTION_BC6H(basic);
DECLARE_COMPRESS_FUNCTION_BC6H(slow);
DECLARE_COMPRESS_FUNCTION_BC6H(veryslow);

#define DECLARE_COMPRESS_FUNCTION_BC7(profile)                              \
    void CompressBlocksBC7_##profile(const rgba_surface* src, uint8_t* dst) \
    {                                                                       \
        bc7_enc_settings settings;                                          \
        GetProfile_##profile(&settings);                                    \
        CompressBlocksBC7(src, dst, &settings);                             \
    }

DECLARE_COMPRESS_FUNCTION_BC7(ultrafast);
DECLARE_COMPRESS_FUNCTION_BC7(veryfast);
DECLARE_COMPRESS_FUNCTION_BC7(fast);
DECLARE_COMPRESS_FUNCTION_BC7(basic);
DECLARE_COMPRESS_FUNCTION_BC7(slow);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_ultrafast);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_veryfast);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_fast);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_basic);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_slow);

bool BCCompression(uint8_t* ppData[MAX_MIPLEVELS], uint8_t* ppOutCompressed[MAX_MIPLEVELS], uint32_t* pCompressedSize,
                   CompressImageDescriptor* pDesc, TextureDesc* pTexDesc)
{
    ASSERT(ppData[0]);
    ASSERT(pDesc);
    ASSERT(pTexDesc->Width && pTexDesc->Height); // Width/Height cannot be 0

    const uint32_t    blockSize = 4;
    BCCompressionFunc bcCompress = CompressBlocksBC7_alpha_fast;
    uint32_t          bytesPerBlock = 16;

    uint32_t inputChannels = TinyImageFormat_ChannelCount((TinyImageFormat)pTexDesc->Format);
    uint32_t requiredInputChannels = 4;
    uint32_t bitsPerPixel = TinyImageFormat_BitSizeOfBlock((TinyImageFormat)pTexDesc->Format);

    //-LDR input is 32 bit / pixel(sRGB), HDR is 64 bit / pixel(half float)
    //	- for BC4 input is 8bit / pixel(R8), for BC5 input is 16bit / pixel(RG8)
    //	- dst buffer must be allocated with enough space for the compressed texture

    switch (pDesc->mDXTCompression)
    {
    case DXT_BC1:
        bcCompress = CompressBlocksBC1;
        bytesPerBlock = 8;
        break;
    case DXT_BC3:
        bcCompress = CompressBlocksBC3;
        break;
    case DXT_BC4:
        bcCompress = CompressBlocksBC4;
        bytesPerBlock = 8;
        requiredInputChannels = 1;
        break;
    case DXT_BC5:
        bcCompress = CompressBlocksBC5;
        requiredInputChannels = 2;
        break;
    case DXT_BC6:
        bcCompress = CompressBlocksBC6H_fast;
        requiredInputChannels = 4;
        if (bitsPerPixel != 64 && !TinyImageFormat_IsFloat((TinyImageFormat)pTexDesc->Format))
        {
            LOGF(LogLevel::Error, "%s is an unsupported format for BC6 compression",
                 TinyImageFormat_Name((TinyImageFormat)pTexDesc->Format));
            return false;
        }
        break;
    case DXT_BC7:
        bcCompress = inputChannels > 3 ? CompressBlocksBC7_alpha_fast : CompressBlocksBC7_fast;
        break;
    default:
        ASSERT(false && "Unknown BC compression Request");
    }
    ASSERT(requiredInputChannels <= inputChannels && "Input should always have more data available");

    // Store if texture mip 0 is padded and use that in the and for the texture descriptor
    uint32_t adjustedWidth = pTexDesc->Width;
    uint32_t adjustedHeight = pTexDesc->Height;
    uint32_t slices = pTexDesc->ArraySize;

    for (uint32_t i = 0; i < pTexDesc->MipLevels; ++i)
    {
        uint32_t Width = max(1u, (pTexDesc->Width >> i));
        uint32_t Height = max(1u, (pTexDesc->Height >> i));
        uint8_t* Data = NULL;

        for (uint32_t slice_index = 0; slice_index < slices; ++slice_index)
        {
            bool     padded = false;
            uint32_t slice_offset = Width * Height * inputChannels * slice_index;

            if (Width % blockSize != 0 || Height % blockSize != 0)
            {
                rgba_surface input;
                input.Width = Width;
                input.Height = Height;
                input.stride = Width * inputChannels;
                input.ptr = ppData[i] + slice_offset;

                uint32_t resizedImageWidth = Width % blockSize == 0 ? Width : Width + (blockSize - Width % blockSize);
                uint32_t resizedImageHeight = Height % blockSize == 0 ? Height : Height + (blockSize - Height % blockSize);

                Data = (uint8_t*)MemAllocate(resizedImageWidth * resizedImageHeight * inputChannels);
                rgba_surface input_padded;
                input_padded.Width = resizedImageWidth;
                input_padded.Height = resizedImageHeight;
                input_padded.stride = resizedImageWidth * inputChannels;
                input_padded.ptr = Data;

                ReplicateBorders(&input_padded, &input, 0, 0, bitsPerPixel);

                Width = resizedImageWidth;
                Height = resizedImageHeight;
                padded = true;

                if (i == 0)
                {
                    adjustedWidth = Width;
                    adjustedHeight = Height;
                }
            }
            else
            {
                Data = ppData[i] + slice_offset;
            }

            const uint32_t xblocks = (Width + blockSize - 1) / blockSize;
            const uint32_t yblocks = (Height + blockSize - 1) / blockSize;

            uint32_t compressed_offset = 0;
            if (ppOutCompressed[i] == NULL)
            {
                pCompressedSize[i] = xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)MemAllocate(pCompressedSize[i]);
            }
            else
            {
                compressed_offset = pCompressedSize[i];
                pCompressedSize[i] += xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)MemReallocate(ppOutCompressed[i], pCompressedSize[i]);
            }

            // Reorder data
            if (requiredInputChannels != inputChannels)
            {
                const uint32_t pixelCount = Width * Height;
                for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
                {
                    uint32_t inputDataIndex = pixelIndex * inputChannels;
                    uint32_t outputDataIndex = pixelIndex * requiredInputChannels;
                    for (uint32_t reorderChannel = 0; reorderChannel < requiredInputChannels; ++reorderChannel)
                    {
                        Data[outputDataIndex + reorderChannel] = Data[inputDataIndex + reorderChannel];
                    }
                }
            }

            rgba_surface input;
            input.Width = Width;
            input.Height = Height;
            input.stride = Width * requiredInputChannels;
            input.ptr = Data;

            bcCompress(&input, ppOutCompressed[i] + compressed_offset);

            if (padded)
            {
                MemDeallocate(Data);
            }
        }
    }

    // Set image Size to padding Size
    pTexDesc->Width = adjustedWidth;
    pTexDesc->Height = adjustedHeight;

    return true;
}

bool CompressImageData(uint8_t* ppData[MAX_MIPLEVELS], uint8_t* ppOutCompressed[MAX_MIPLEVELS], uint32_t* pCompressedSize,
                       CompressImageDescriptor* pDesc, TextureDesc* pTextDesc)
{
    if (!ppData[0])
    {
        ppOutCompressed[0] = NULL;
        pCompressedSize[0] = 0;
        return false;
    }

    switch (pDesc->mCompression)
    {
    case COMPRESSION_ASTC:
        return ASTCCompression(ppData, ppOutCompressed, pCompressedSize, pDesc, pTextDesc);
    case COMPRESSION_BC:
        return BCCompression(ppData, ppOutCompressed, pCompressedSize, pDesc, pTextDesc);
    default:
        LOGF(Error, "Unknown compression!");
        return false;
    }
}

void GenerateMipmaps(uint8_t* ppData[MAX_MIPLEVELS], uint32_t* pImageDataSize, TextureDesc* pTextDesc)
{
    uint32_t Width = pTextDesc->Width;
    uint32_t Height = pTextDesc->Height;
    uint32_t numLevels = max((uint32_t)log2(Width), (uint32_t)log2(Height)) + 1u;
    uint32_t channels = TinyImageFormat_ChannelCount((TinyImageFormat)pTextDesc->Format);

    for (uint32_t i = 1; i < numLevels; ++i)
    {
        uint32_t prevWidth = max(Width >> (i - 1u), 1u);
        uint32_t prevHeight = max(Height >> (i - 1u), 1u);
        uint32_t mipWidth = max(Width >> i, 1u);
        uint32_t mipHeight = max(Height >> i, 1u);

        ppData[i] = (uint8_t*)MemAllocate(mipWidth * mipHeight * channels);
        pImageDataSize[i] = mipWidth * mipHeight * channels;

        uint8_t* inImageData = ppData[i - 1];
        uint8_t* outImageData = ppData[i];

        // TODO:
        //  - different color spaces
        //  - other filter options
        //  - other clamping ways
        int result = stbir_resize_uint8_generic(
            inImageData, prevWidth, prevHeight, channels * prevWidth, outImageData, mipWidth, mipHeight, channels * mipWidth, channels,
            STBIR_ALPHA_CHANNEL_NONE, STBIR_FLAG_ALPHA_USES_COLORSPACE, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
            TinyImageFormat_IsSRGB((TinyImageFormat)pTextDesc->Format) ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR, nullptr);

        ASSERT(result == 1);
    }

    pTextDesc->MipLevels = numLevels;
}

void GenerateVMFFilteredMipmaps(uint8_t* ppData[MAX_MIPLEVELS], uint32_t* pImageDataSize, TextureDesc* pTextureDesc, uint32_t channelCount,
                                void* UserData)
{
    UNREF_PARAM(pImageDataSize);
    ASSERT(channelCount > 2 && "Minimum of 3 channels input is required");

    const uint32_t Width = pTextureDesc->Width;
    const uint32_t Height = pTextureDesc->Height;
    const uint32_t numLevels = max((uint32_t)log2(Width), (uint32_t)log2(Height)) + 1u;
    pTextureDesc->MipLevels = numLevels;

    Vector3* rData[MAX_MIPLEVELS];
    rData[0] = (Vector3*)UserData;

    for (uint32_t mipMap = 1; mipMap < numLevels; ++mipMap)
    {
        uint32_t prevWidth = max(Width >> (mipMap - 1u), 1u);
        uint32_t prevHeight = max(Height >> (mipMap - 1u), 1u);
        uint32_t mipWidth = max(Width >> mipMap, 1u);
        uint32_t mipHeight = max(Height >> mipMap, 1u);
        rData[mipMap] = (Vector3*)MemAllocate(mipWidth * mipHeight * sizeof(Vector3));
        ppData[mipMap] = (uint8_t*)MemAllocate(mipWidth * mipHeight * channelCount);

        for (uint32_t y = 0; y < mipHeight; ++y)
        {
            for (uint32_t x = 0; x < mipWidth; ++x)
            {
                const uint32_t mipPixelIndex = x + y * mipWidth;
                const float    xCenter = clamp((x << 1) + 0.5f, 0.0f, prevWidth - 1.0f);
                const float    yCenter = clamp((y << 1) + 0.5f, 0.0f, prevHeight - 1.0f);
                float          xl = floorf(xCenter);
                float          xu = ceilf(xCenter);
                float          yl = floorf(yCenter);
                float          yu = ceilf(yCenter);

                // bilinear interpolation
                Vector3 xlValue = rData[mipMap - 1][(uint32_t)(xl + yl * prevWidth)];
                Vector3 xuValue = rData[mipMap - 1][(uint32_t)(xu + yl * prevWidth)];

                Vector3 ylValue = rData[mipMap - 1][(uint32_t)(xl + yu * prevWidth)];
                Vector3 yuValue = rData[mipMap - 1][(uint32_t)(xu + yu * prevWidth)];

                Vector3 horizontalL = xlValue * (xu - xCenter) / (xu - xl) + xuValue * (xCenter - xl) / (xu - xl);
                Vector3 horizontalU = ylValue * (xu - xCenter) / (xu - xl) + yuValue * (xCenter - xl) / (xu - xl);

                Vector3 rFiltered = horizontalL * (yu - yCenter) / (yu - yl) + horizontalU * (yCenter - yl) / (yu - yl);

                rData[mipMap][mipPixelIndex] = rFiltered;
                rFiltered = normalize(rFiltered);
                // Move normal back to range [0, 1]
                rFiltered = rFiltered * 0.5f + Vector3(0.5f);

                const uint32_t mipChannelIndex = mipPixelIndex * channelCount;
                ppData[mipMap][mipChannelIndex + 0] = (uint8_t)(fmaxf(fminf(1.0f, rFiltered[0]), 0.0f) * (float)UINT8_MAX + 0.5f);
                ppData[mipMap][mipChannelIndex + 1] = (uint8_t)(fmaxf(fminf(1.0f, rFiltered[1]), 0.0f) * (float)UINT8_MAX + 0.5f);
                ppData[mipMap][mipChannelIndex + 3] = (uint8_t)(fmaxf(fminf(1.0f, rFiltered[2]), 0.0f) * (float)UINT8_MAX + 0.5f);
            }
        }
    }

    for (uint32_t mipMap = 1; mipMap < numLevels; ++mipMap)
    {
        MemDeallocate(rData[mipMap]);
    }
}

bool GenerateVMFLayer(InputTextureData* pNormalTextureData, InputTextureData* pRoughnessTextureData, Vector3* rData)
{
    ASSERT(pNormalTextureData);
    ASSERT(pRoughnessTextureData);

    bool success = true;

    /////////////////////////////////
    // Validate input
    /////////////////////////////////
    if (pNormalTextureData->isCompressed || pRoughnessTextureData->isCompressed)
    {
        LOGF(LogLevel::Error, "%s: Compressed input data given, requires uncompressed texture data!", __FUNCTION__);
        success = false;
    }

    if (pNormalTextureData->mDesc.Width != pRoughnessTextureData->mDesc.Width ||
        pNormalTextureData->mDesc.Height != pRoughnessTextureData->mDesc.Height)
    {
        LOGF(LogLevel::Error, "%s: width/height of normal {%u/%u} and roughness {%u/%u} texture do not match!", __FUNCTION__,
             pNormalTextureData->mDesc.Width, pNormalTextureData->mDesc.Height, pRoughnessTextureData->mDesc.Width,
             pRoughnessTextureData->mDesc.Height);

        success = false;
    }

    const uint32_t normalTextureChannels = TinyImageFormat_ChannelCount((TinyImageFormat)pNormalTextureData->mDesc.Format);
    const uint32_t roughnessTextureChannels = TinyImageFormat_ChannelCount((TinyImageFormat)pRoughnessTextureData->mDesc.Format);
    if (normalTextureChannels < 3)
    {
        LOGF(LogLevel::Error, "%s: Normal input texture has to few channels %u!", __FUNCTION__, normalTextureChannels);
        success = false;
    }

    /////////////////////////////////
    // Generate layer
    /////////////////////////////////
    if (success)
    {
        for (uint32_t y = 0; y < pNormalTextureData->mDesc.Height; ++y)
        {
            for (uint32_t x = 0; x < pNormalTextureData->mDesc.Width; ++x)
            {
                const uint32_t pixelIndex = x + y * pNormalTextureData->mDesc.Width;
                const uint32_t pixelIndexNormal = pixelIndex * normalTextureChannels;
                const uint32_t pixelIndexRoughness = pixelIndex * roughnessTextureChannels;

                Vector3 normal = Vector3(pNormalTextureData->Data[0][pixelIndexNormal + 0] / (float)UINT8_MAX,
                                         pNormalTextureData->Data[0][pixelIndexNormal + 1] / (float)UINT8_MAX,
                                         pNormalTextureData->Data[0][pixelIndexNormal + 2] / (float)UINT8_MAX);

                // Set normal to range [-1.0, 1.0]
                normal = normal * 2.0f - Vector3(1.0f);

                // TODO Optional: Give channel to look into for the rougness value as argument
                // For now expect the first channel to contain the actuall roughness value
                const float roughnessValue = pRoughnessTextureData->Data[0][pixelIndexRoughness + 0] / (float)UINT8_MAX;

                // Convert to r form
                const float invLambda = 0.5f * roughnessValue * roughnessValue;
                const float exp2l = exp(-2.0f / invLambda);
                const float cothLambda = invLambda > 0.1f ? (1.0f + exp2l) / (1.0f - exp2l) : 1.0f;
                Vector3     r = normal * (cothLambda - invLambda);
                rData[pixelIndex] = r;
            }
        }
    }

    return success;
}

void onTextureFound(ResourceDirectory resourceDir, const char* filename, void* UserData)
{
    UNREF_PARAM(resourceDir);
    bstring** fileNames = (bstring**)UserData;
    arrpush(*fileNames, bdynfromcstr(filename));
}

bool ProcessTextures(AssetPipelineParams* assetParams, ProcessTexturesParams* texturesParams)
{
    // TODO:
    //  - Texture arrays
    //  - Cubemaps
    //  - HDR texture support

    bool     error = false;
    // Get all image files
    bstring* inputImgFileNames = NULL;

    if (assetParams->mPathMode == PROCESS_MODE_FILE)
    {
        arrpush(inputImgFileNames, bdynfromcstr(assetParams->mInFilePath));
    }
    else
    {
        DirectorySearch(assetParams->mRDInput, NULL, texturesParams->mInExt, onTextureFound, (void*)&inputImgFileNames,
                        assetParams->mPathMode == PROCESS_MODE_DIRECTORY_RECURSIVE);
    }

    uint32_t imgFileCount = (uint32_t)arrlenu(inputImgFileNames);

    for (uint32_t i = 0; i < imgFileCount; ++i)
    {
        hz::Format            outFormat = hz::Format::UNDEFINED;
        ProcessTexturesParams copyTextureParams = *texturesParams;
        bool                  useVMF = copyTextureParams.pRoughnessFilePath != NULL;

        const char* inFileName = (char*)inputImgFileNames[i].data;

        char inExtension[FS_MAX_PATH] = { 0 };
        GetPathExtension(inFileName, inExtension);

        char outFileName[FS_MAX_PATH] = { 0 };

        if (assetParams->mOutSubdir)
        {
            char fileName[FS_MAX_PATH] = {};
            GetPathFileName(inFileName, fileName);

            strcat(fileName, ".tex");

            AppendPathComponent(assetParams->mOutSubdir, fileName, outFileName);
        }
        else
        {
            ReplacePathExtension(inFileName, "tex", outFileName);
        }

        // If input file newer than output file redo compression
        if (!assetParams->mSettings.force && FileExists(assetParams->mRDOutput, outFileName))
        {
            time_t lastModified = GetLastModifiedTime(assetParams->mRDInput, inFileName);
            if (assetParams->mAdditionalModifiedTime != 0)
                lastModified = max(lastModified, assetParams->mAdditionalModifiedTime);

            time_t lastProcessed = GetLastModifiedTime(assetParams->mRDOutput, outFileName);

            if (lastModified < lastProcessed)
            {
                LOGF(Info, "Skipping %s", inFileName);
                continue;
            }
        }

        LOGF(Info, "Converting texture %s from .%s to .%s with output container : %s", inFileName, copyTextureParams.mInExt, "tex",
             gExtensions[copyTextureParams.mContainer]);

        /////////////////////////////////
        // Load raw image data
        ////////////////////////////////
        InputTextureData inputTextureData = {};
        if (!LoadTextureData(assetParams->mRDInput, inFileName, inExtension, &copyTextureParams, &inputTextureData))
        {
            error = true;
            continue;
        }

        /////////////////////////////////
        // vMF
        /////////////////////////////////
        Vector3* rData = nullptr;

        if (useVMF)
        {
            InputTextureData inputRoughnessTextureData = {};
            if (!LoadTextureData(assetParams->mRDInput, copyTextureParams.pRoughnessFilePath, inExtension, &copyTextureParams,
                                 &inputRoughnessTextureData))
            {
                error = true;
                continue;
            }

            rData = (Vector3*)MemAllocate(inputTextureData.mDesc.Width * inputTextureData.mDesc.Height * sizeof(Vector3));

            if (!GenerateVMFLayer(&inputTextureData, &inputRoughnessTextureData, rData))
            {
                error = true;
            }

            // Release rougness texture data
            for (size_t mip = 0; mip < inputTextureData.mDesc.MipLevels; ++mip)
            {
                MemDeallocate(inputRoughnessTextureData.Data[mip]);
                inputRoughnessTextureData.Data[mip] = NULL;
                inputRoughnessTextureData.mDataSize[mip] = 0;
            }

            copyTextureParams.pCallbackUserData = rData;
            copyTextureParams.mGenerateMipmaps = TextureMipmap::MIPMAP_CUSTOM;
            copyTextureParams.pGenerateMipmapsCallback = GenerateVMFFilteredMipmaps;

            if (copyTextureParams.mCompression == COMPRESSION_BC && copyTextureParams.mOverrideBC == DXT_NONE)
            {
                LOGF(Info, "Using DXT_BC5 compression for vMF output");
                copyTextureParams.mOverrideBC = DXT_BC5;
            }
        }

        /////////////////////////////////
        // Generate mipmaps
        /////////////////////////////////
        if (copyTextureParams.mGenerateMipmaps == MIPMAP_CUSTOM)
        {
            ASSERT(copyTextureParams.pGenerateMipmapsCallback && "MIPMAP_CUSTOM requires pGenerateMipmapsCallback to be Set");
            if (inputTextureData.Data[0])
            {
                uint32_t channels = TinyImageFormat_ChannelCount((TinyImageFormat)inputTextureData.mDesc.Format);
                copyTextureParams.pGenerateMipmapsCallback(inputTextureData.Data, inputTextureData.mDataSize, &inputTextureData.mDesc,
                                                           channels, copyTextureParams.pCallbackUserData);
            }
        }

        if (copyTextureParams.mGenerateMipmaps == MIPMAP_DEFAULT && inputTextureData.mDesc.MipLevels <= 1 && !inputTextureData.isCompressed)
        {
            if (inputTextureData.Data[0])
            {
                GenerateMipmaps(inputTextureData.Data, inputTextureData.mDataSize, &inputTextureData.mDesc);
            }
        }

        if (useVMF)
        {
            MemDeallocate(rData);
        }

        /////////////////////////////////
        // Compress
        /////////////////////////////////
        CompressImageDescriptor compressDesc = {};

        if (inputTextureData.isCompressed)
        {
            outFormat = inputTextureData.mDesc.Format;
            LOGF(Warning, "Input texture '%s' is already compressed {%s}, copy texture to destination", inFileName,
                 TinyImageFormat_Name((TinyImageFormat)outFormat));
        }
        else
        {
            outFormat = GetOutputTextureFormat(&copyTextureParams, &inputTextureData.mDesc, &compressDesc);
        }

        uint8_t* pCompressedData[MAX_MIPLEVELS] = { NULL };
        uint32_t compressedDataSize[MAX_MIPLEVELS] = { 0 };

        if (outFormat == hz::Format::UNDEFINED)
        {
            LOGF(Error, "Undefined Image format");
            error = true;
            continue;
        }

        if (!inputTextureData.isCompressed && copyTextureParams.mCompression != TextureCompression::COMPRESSION_NONE)
        {
            // Process raw image data
            if (!CompressImageData(inputTextureData.Data, pCompressedData, compressedDataSize, &compressDesc, &inputTextureData.mDesc))
            {
                LOGF(Error, "Failed to compress texture %s", inFileName);
                error = true;
            }

            // Free raw image data, can be released once image is compressed
            if (inputTextureData.Data[0])
            {
                // Release image data
                for (uint32_t mip = 0; mip < inputTextureData.mDesc.MipLevels; ++mip)
                {
                    stbi_image_free(inputTextureData.Data[mip]);
                    inputTextureData.Data[mip] = NULL;
                }
            }
        }
        else
        {
            // Set raw pImageData as out data
            for (uint32_t mip = 0; mip < inputTextureData.mDesc.MipLevels; ++mip)
            {
                pCompressedData[mip] = inputTextureData.Data[mip];
                compressedDataSize[mip] = inputTextureData.mDataSize[mip];
            }
        }

        /////////////////////////////////
        // Write output
        /////////////////////////////////

        // Remove old file
        if (!error)
        {
            RemoveFile(assetParams->mRDOutput, outFileName);
        }

        // Make sure output folder exists
        {
            char assetPath[FS_MAX_PATH] = {};
            GetParentPath(outFileName, assetPath);
            MakeDirectory(assetParams->mRDOutput, assetPath, true);
        }

        FileStream outFile = {};
        if (!OpenStreamFromPath(assetParams->mRDOutput, outFileName, FileModeWrite, &outFile))
        {
            LOGF(Error, "Could not open file '%s' for write.", outFileName);
            error = true;
        }

        // Write .ktx file
        const bool     isCubemap = (inputTextureData.mDesc.Descriptors & DescriptorTypeTextureCube) == DescriptorTypeTextureCube;
        // Array Size in the disk image needs to be 1 since we'll already multiply it by 6 when loading the texture in runtime
        const uint32_t ArraySize = isCubemap ? inputTextureData.mDesc.ArraySize / 6 : inputTextureData.mDesc.ArraySize;
        if (copyTextureParams.mContainer == CONTAINER_KTX)
        {
            TinyKtx_Format outKtxFormat = TinyImageFormat_ToTinyKtxFormat((TinyImageFormat)outFormat);
            if (!TinyKtx_WriteImage(&ktxWriteCallbacks, &outFile, inputTextureData.mDesc.Width, inputTextureData.mDesc.Height,
                                    inputTextureData.mDesc.Depth, ArraySize, inputTextureData.mDesc.MipLevels, outKtxFormat, isCubemap,
                                    compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(Error, "Couldn't create ktx file '%s' with format '%s'", outFileName,
                     TinyImageFormat_Name((TinyImageFormat)outFormat));
                error = true;
            }
        }
        // Write .dds file
        else if (copyTextureParams.mContainer == CONTAINER_DDS)
        {
            TinyDDS_Format outDDSFormat = TinyImageFormat_ToTinyDDSFormat((TinyImageFormat)outFormat);
            if (!TinyDDS_WriteImage(&ddsWriteCallbacks, &outFile, inputTextureData.mDesc.Width, inputTextureData.mDesc.Height,
                                    inputTextureData.mDesc.Depth, ArraySize, inputTextureData.mDesc.MipLevels, outDDSFormat, isCubemap,
                                    false, compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(Error, "Couldn't create dds file '%s' with format '%s'", outFileName,
                     TinyImageFormat_Name((TinyImageFormat)outFormat));
                error = true;
            }
        }
#ifdef XBOX_SCARLETT_DDS
        else if (copyTextureParams.mContainer == CONTAINER_SCARLETT_DDS)
        {
            extern bool swizzleAndWriteDds(TinyDDS_WriteCallbacks const* callbacks, void* user, uint32_t Width, uint32_t Height,
                                           uint32_t Depth, uint32_t slices, uint32_t mipmaplevels, TinyDDS_Format Format, bool cubemap,
                                           uint32_t const* mipmapsizes, void const** mipmaps);

            TinyDDS_Format outDDSFormat = TinyImageFormat_ToTinyDDSFormat((TinyImageFormat)outFormat);
            if (!swizzleAndWriteDds(&ddsWriteCallbacks, &outFile, inputTextureData.mDesc.Width, inputTextureData.mDesc.Height,
                                    inputTextureData.mDesc.Depth, inputTextureData.mDesc.ArraySize, inputTextureData.mDesc.MipLevels,
                                    outDDSFormat, isCubemap, compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(Error, "Couldn't create Scarlett dds file '%s' with format '%s'", outFileName,
                     TinyImageFormat_Name((TinyImageFormat)outFormat));
                error = true;
            }
        }
#endif
#ifdef PROSPERO_GNF
        else if (copyTextureParams.mContainer == CONTAINER_GNF_ORBIS || copyTextureParams.mContainer == CONTAINER_GNF_PROSPERO)
        {
            extern bool writeGnfTexture(FileStream * outFile, uint32_t Width, uint32_t Height, uint32_t Depth, uint32_t slices,
                                        uint32_t mipmaplevels, hz::Format Format, bool cubemap, TextureContainer outTexContainer,
                                        uint32_t tilingQuality, uint32_t const* mipmapsizes, void const** mipmaps);

            if (!writeGnfTexture(&outFile, inputTextureData.mDesc.Width, inputTextureData.mDesc.Height, inputTextureData.mDesc.Depth,
                                 inputTextureData.mDesc.ArraySize, inputTextureData.mDesc.MipLevels, outFormat, isCubemap,
                                 copyTextureParams.mContainer, 1, compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(Error, "Couldn't create gnf file '%s' with format '%s'", outFileName,
                     TinyImageFormat_Name((TinyImageFormat)outFormat));
                error = true;
            }
        }
#endif
        else
        {
            ASSERT(false && "No supported output extension");
        }

        // Close out file stream
        CloseStream(&outFile);

        if (pCompressedData[0])
        {
            for (size_t mip = 0; mip < inputTextureData.mDesc.MipLevels; ++mip)
            {
                MemDeallocate(pCompressedData[mip]);
                pCompressedData[mip] = NULL;
                compressedDataSize[mip] = 0;
            }
        }

        if (copyTextureParams.ppOutProcessedTextureData)
        {
            ProcessedTextureData outData = {};
            outData.mOutputFilePath = bdynfromcstr(outFileName);
            outData.Width = inputTextureData.mDesc.Width;
            outData.Height = inputTextureData.mDesc.Height;
            outData.Depth = inputTextureData.mDesc.Depth;
            outData.ArraySize = inputTextureData.mDesc.ArraySize;
            outData.MipLevels = inputTextureData.mDesc.MipLevels;
            outData.Format = (uint32_t)inputTextureData.mDesc.Format;
            arrpush(*copyTextureParams.ppOutProcessedTextureData, outData);
        }

        // Remove the output file if it was created but process textures failed.
        if (error)
        {
            RemoveFile(assetParams->mRDOutput, outFileName);
        }
    }

    if (inputImgFileNames)
    {
        for (uint32_t i = 0; i < imgFileCount; ++i)
        {
            bdestroy(&inputImgFileNames[i]);
        }

        arrfree(inputImgFileNames);
        inputImgFileNames = NULL;
    }

    return error;
}
