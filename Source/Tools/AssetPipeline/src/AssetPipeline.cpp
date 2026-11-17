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

#include "AssetPipeline.h"
#include "AssetPipeline/IAssetPipeline.h"
#include "SceneAssetCooker.h"
#include "CookUtilities.h"

// Math
#include <ThirdParty/ModifiedSonyMath1/vectormath.hpp>

#include "Runtime/Core/Private/Math/ShaderUtilities.h"

#include <ThirdParty/tinyimageformat/tinyimageformat_base.h>

// Meshoptimizer
#include <ThirdParty/meshoptimizer/src/meshoptimizer.h>

#ifdef ENABLE_ASSET_PIPELINE_CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#endif
#ifdef ENABLE_ASSET_PIPELINE_CGLTF_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#endif
// We are using jsmn to Parse joints, these functions are static so it doesn't matter if they are defined in other files.
#define CGLTF_JSMN_IMPLEMENTATION
#include <ThirdParty/cgltf/cgltf_write.h>
#include <ThirdParty/cJSON/cJSON.h>

#ifdef ENABLE_ASSET_PIPELINE_TINYDDS_IMPLEMENTATION
#define TINYDDS_IMPLEMENTATION
#endif
#ifdef ENABLE_ASSET_PIPELINE_TINYKTX_IMPLEMENTATION
#define TINYKTX_IMPLEMENTATION
#endif
#include "Platform/IOperatingSystem.h"
#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"

#include "Runtime/Resources/Private/ResourceLoader/TextureContainers.h"

#include "Core/IMemory.h" //NOTE: this should be the last include in a .cpp

static void BeginAssetPipelineSection(const char* section) { LOGF(Info, "========== %s ==========", section); }

static void EndAssetPipelineSection(const char* section) { LOGF(Info, "========== !%s ==========", section); }

// The geometry cook indexes per-semantic tables with these attributes and indexes skin extras by joint count, so
// unsupported input must be rejected here instead of asserting there.
static bool validateSceneAssetCookInput(const cgltf_data* Data)
{
    for (cgltf_size i = 0; i < Data->meshes_count; ++i)
    {
        for (cgltf_size j = 0; j < Data->meshes[i].primitives_count; ++j)
        {
            const cgltf_primitive& primitive = Data->meshes[i].primitives[j];
            if (primitive.type != cgltf_primitive_type_triangles)
            {
                LOGF(Error, "Scene mesh %u primitive %u requires triangle-list topology", (uint32_t)i, (uint32_t)j);
                return false;
            }
            if (!primitive.attributes_count)
            {
                LOGF(Error, "Scene mesh %u primitive %u has no vertex attributes", (uint32_t)i, (uint32_t)j);
                return false;
            }
            for (cgltf_size k = 0; k < primitive.attributes_count; ++k)
            {
                const cgltf_attribute& attribute = primitive.attributes[k];
                // Application-specific attributes start with an underscore; the geometry cook skips them.
                if (attribute.type == cgltf_attribute_type_invalid && attribute.name && attribute.name[0] == '_')
                {
                    continue;
                }
                const bool indexed = attribute.type == cgltf_attribute_type_texcoord;
                if (attribute.type == cgltf_attribute_type_invalid ||
                    (indexed ? attribute.index > SEMANTIC_TEXCOORD9 - SEMANTIC_TEXCOORD0 : attribute.index != 0))
                {
                    LOGF(Error, "Scene mesh %u primitive %u has unsupported vertex attribute '%s'", (uint32_t)i, (uint32_t)j,
                         attribute.name ? attribute.name : "");
                    return false;
                }
            }
        }
    }
    for (cgltf_size i = 0; i < Data->skins_count; ++i)
    {
        const cgltf_skin& skin = Data->skins[i];
        if (skin.extras.end_offset > skin.extras.start_offset && !skin.inverse_bind_matrices)
        {
            LOGF(Error, "Scene skin %u has joint remaps without inverse bind matrices", (uint32_t)i);
            return false;
        }
    }
    for (cgltf_size i = 0; i < Data->textures_count; ++i)
    {
        const cgltf_image* pImage = Data->textures[i].image;
        if (!pImage || (!pImage->uri && !pImage->buffer_view) || (pImage->uri && pImage->buffer_view))
        {
            LOGF(Error, "Scene texture %u must reference an image URI or buffer view", (uint32_t)i);
            return false;
        }
    }
    return true;
}

static const uint32_t kSceneAssetCookMaxDependencies = 512;
static const uint32_t kSceneAssetCookHashCapacity = 32;

struct SceneAssetCookFingerprint
{
    uint64_t mHash;
    uint32_t mDependencyCount;
    char     mDependencies[kSceneAssetCookMaxDependencies][FS_MAX_PATH];
};

static bool addSceneAssetCookDependency(SceneAssetCookFingerprint* pFingerprint, const char* pPath, const void* Data, size_t Size)
{
    char normalizedPath[FS_MAX_PATH] = {};
    NormalizePath(pPath, '/', normalizedPath);
    for (uint32_t i = 0; i < pFingerprint->mDependencyCount; ++i)
    {
        if (strcmp(pFingerprint->mDependencies[i], normalizedPath) == 0)
            return true;
    }
    if (!normalizedPath[0] || strlen(normalizedPath) >= FS_MAX_PATH || pFingerprint->mDependencyCount >= kSceneAssetCookMaxDependencies)
    {
        return false;
    }

    snprintf(pFingerprint->mDependencies[pFingerprint->mDependencyCount++], FS_MAX_PATH, "%s", normalizedPath);
    pFingerprint->mHash = cookHash(normalizedPath, strlen(normalizedPath) + 1, pFingerprint->mHash);
    pFingerprint->mHash = cookHash(Data, Size, pFingerprint->mHash);
    return true;
}

static bool addSceneAssetCookStatDependency(ResourceDirectory resourceDirectory, SceneAssetCookFingerprint* pFingerprint, const char* pPath,
                                            uint64_t Size)
{
    char normalizedPath[FS_MAX_PATH] = {};
    NormalizePath(pPath, '/', normalizedPath);
    for (uint32_t i = 0; i < pFingerprint->mDependencyCount; ++i)
    {
        if (strcmp(pFingerprint->mDependencies[i], normalizedPath) == 0)
        {
            return true;
        }
    }
    if (!normalizedPath[0] || strlen(normalizedPath) >= FS_MAX_PATH || pFingerprint->mDependencyCount >= kSceneAssetCookMaxDependencies)
    {
        return false;
    }

    const time_t modified = GetLastModifiedTime(resourceDirectory, pPath);
    if (!modified)
    {
        return false;
    }

    snprintf(pFingerprint->mDependencies[pFingerprint->mDependencyCount++], FS_MAX_PATH, "%s", normalizedPath);
    pFingerprint->mHash = cookHash(normalizedPath, strlen(normalizedPath) + 1, pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&modified, sizeof(modified), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&Size, sizeof(Size), pFingerprint->mHash);
    return true;
}

static bool buildSceneAssetCookFingerprint(ResourceDirectory resourceDirectory, const char* pSourcePath, const void* pSourceData,
                                           size_t sourceSize, const cgltf_data* Data, const ProcessGLTFParams* pParams,
                                           time_t additionalModifiedTime, SceneAssetCookFingerprint* pFingerprint)
{
    static const uint32_t kCookerRevision = 12;
    pFingerprint->mHash = kCookHashSeed;
    pFingerprint->mHash = cookHash(&kCookerRevision, sizeof(kCookerRevision), pFingerprint->mHash);
    // Output written for another geometry format or struct layout must be recooked, not just rejected at load.
    pFingerprint->mHash = cookHash(&GEOMETRY_FILE_VERSION, sizeof(GEOMETRY_FILE_VERSION), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(GEOMETRY_FILE_LAYOUT, sizeof(GEOMETRY_FILE_LAYOUT), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&additionalModifiedTime, sizeof(additionalModifiedTime), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(pParams->pVertexLayout, sizeof(*pParams->pVertexLayout), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&pParams->mIgnoreMissingAttributes, sizeof(pParams->mIgnoreMissingAttributes), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&pParams->mProcessMeshlets, sizeof(pParams->mProcessMeshlets), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&pParams->mNumMaxVertices, sizeof(pParams->mNumMaxVertices), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&pParams->mNumMaxTriangles, sizeof(pParams->mNumMaxTriangles), pFingerprint->mHash);
    pFingerprint->mHash = cookHash(&pParams->mOptimizationFlags, sizeof(pParams->mOptimizationFlags), pFingerprint->mHash);
    if (!addSceneAssetCookDependency(pFingerprint, pSourcePath, pSourceData, sourceSize))
        return false;

    char sourceParent[FS_MAX_PATH] = {};
    GetParentPath(pSourcePath, sourceParent);
    for (cgltf_size i = 0; i < Data->buffers_count; ++i)
    {
        const cgltf_buffer* pBuffer = &Data->buffers[i];
        if (!pBuffer->uri || strncmp(pBuffer->uri, "data:", 5) == 0 || strstr(pBuffer->uri, "://"))
            continue;
        char dependencyPath[FS_MAX_PATH] = {};
        AppendPathComponent(sourceParent, pBuffer->uri, dependencyPath);
        // Buffers are already loaded for the cook, so their bytes cost nothing extra to hash and catch edits that keep the
        // modification time.
        if (!addSceneAssetCookDependency(pFingerprint, dependencyPath, pBuffer->data, pBuffer->size))
        {
            return false;
        }
    }
    for (cgltf_size i = 0; i < Data->images_count; ++i)
    {
        const cgltf_image* pImage = &Data->images[i];
        if (!pImage->uri || strncmp(pImage->uri, "data:", 5) == 0 || strstr(pImage->uri, "://"))
            continue;
        char dependencyPath[FS_MAX_PATH] = {};
        AppendPathComponent(sourceParent, pImage->uri, dependencyPath);
        // Scene images can total gigabytes, so the startup check hashes their size and modification time, not their bytes.
        FileStream image = {};
        if (!OpenStreamFromPath(resourceDirectory, dependencyPath, FileModeRead, &image))
        {
            return false;
        }
        const ssize_t imageSize = GetStreamFileSize(&image);
        CloseStream(&image);
        if (imageSize < 0 || !addSceneAssetCookStatDependency(resourceDirectory, pFingerprint, dependencyPath, (uint64_t)imageSize))
        {
            return false;
        }
    }
    return true;
}

static void formatSceneAssetCookHash(const SceneAssetCookFingerprint* pFingerprint, char output[kSceneAssetCookHashCapacity])
{
    snprintf(output, kSceneAssetCookHashCapacity, "fnv1a64:%016llx", (unsigned long long)pFingerprint->mHash);
}

void CreateDirectoryForFile(ResourceDirectory resourceDir, const char* filename)
{
    char directory[FS_MAX_PATH] = {};
    GetParentPath(filename, directory);
    MakeDirectory(resourceDir, directory, true);
}

void DirectorySearch(ResourceDirectory resourceDir, const char* subDir, const char* ext, OnFind onFindCallback, void* UserData,
                     bool recursive)
{
    // LOGF(Debug, "Exploring subdir: %s", subDir ? subDir : "Root");

    // Get all requested extension files
    char** filesInDirectory = NULL;
    int    filesFoundInDirectory = 0;
    GetFilesWithExtension(resourceDir, subDir ? subDir : "", ext, &filesInDirectory, &filesFoundInDirectory);
    for (int j = 0; j < filesFoundInDirectory; ++j)
    {
        if (filesInDirectory[j])
        {
            const char* file = filesInDirectory[j]; //-V595
            // LOGF(Debug, "Found file: %s", file);

            onFindCallback(resourceDir, file, UserData);
        }
    }

    if (filesInDirectory != NULL) //-V595
    {
        MemDeallocate(filesInDirectory);
    }

    if (recursive)
    {
        char** subDirectories = NULL;
        int    subDirectoryCount = 0;
        GetSubDirectories(resourceDir, subDir ? subDir : "", &subDirectories, &subDirectoryCount);
        for (int i = 0; i < subDirectoryCount; ++i)
        {
            if (subDirectories[i])
            {
                const char* sDir = subDirectories[i]; //-V595
                DirectorySearch(resourceDir, sDir, ext, onFindCallback, UserData, recursive);
            }
        }

        if (subDirectories != NULL) //-V595
        {
            MemDeallocate(subDirectories);
        }
    }
}

static inline constexpr ShaderSemantic util_cgltf_attrib_type_to_semantic(cgltf_attribute_type Type, uint32_t index)
{
    switch (Type)
    {
    case cgltf_attribute_type_position:
        return SEMANTIC_POSITION;
    case cgltf_attribute_type_normal:
        return SEMANTIC_NORMAL;
    case cgltf_attribute_type_tangent:
        return SEMANTIC_TANGENT;
    case cgltf_attribute_type_color:
        return SEMANTIC_COLOR;
    case cgltf_attribute_type_joints:
        return SEMANTIC_JOINTS;
    case cgltf_attribute_type_weights:
        return SEMANTIC_WEIGHTS;
    case cgltf_attribute_type_texcoord:
        return (ShaderSemantic)(SEMANTIC_TEXCOORD0 + index);
    default:
        // Application-specific attributes; validateSceneAssetCookInput rejects every other kind.
        return SEMANTIC_UNDEFINED;
    }
}

static inline constexpr hz::Format util_cgltf_type_to_image_format(cgltf_type Type, cgltf_component_type compType)
{
    switch (Type)
    {
    case cgltf_type_scalar:
        if (cgltf_component_type_r_8 == compType)
            return hz::Format::R8_SINT;
        else if (cgltf_component_type_r_16 == compType)
            return hz::Format::R16_SINT;
        else if (cgltf_component_type_r_16u == compType)
            return hz::Format::R16_UINT;
        else if (cgltf_component_type_r_32f == compType)
            return hz::Format::R32_SFLOAT;
        else if (cgltf_component_type_r_32u == compType)
            return hz::Format::R32_UINT;
    case cgltf_type_vec2:
        if (cgltf_component_type_r_8 == compType)
            return hz::Format::R8G8_SINT;
        else if (cgltf_component_type_r_16 == compType)
            return hz::Format::R16G16_SINT;
        else if (cgltf_component_type_r_16u == compType)
            return hz::Format::R16G16_UINT;
        else if (cgltf_component_type_r_32f == compType)
            return hz::Format::R32G32_SFLOAT;
        else if (cgltf_component_type_r_32u == compType)
            return hz::Format::R32G32_UINT;
    case cgltf_type_vec3:
        if (cgltf_component_type_r_8 == compType)
            return hz::Format::R8G8B8_SINT;
        else if (cgltf_component_type_r_16 == compType)
            return hz::Format::R16G16B16_SINT;
        else if (cgltf_component_type_r_16u == compType)
            return hz::Format::R16G16B16_UINT;
        else if (cgltf_component_type_r_32f == compType)
            return hz::Format::R32G32B32_SFLOAT;
        else if (cgltf_component_type_r_32u == compType)
            return hz::Format::R32G32B32_UINT;
    case cgltf_type_vec4:
        if (cgltf_component_type_r_8 == compType)
            return hz::Format::R8G8B8A8_SINT;
        else if (cgltf_component_type_r_8u == compType)
            return hz::Format::R8G8B8A8_UINT;
        else if (cgltf_component_type_r_16 == compType)
            return hz::Format::R16G16B16A16_SINT;
        else if (cgltf_component_type_r_16u == compType)
            return hz::Format::R16G16B16A16_UINT;
        else if (cgltf_component_type_r_32f == compType)
            return hz::Format::R32G32B32A32_SFLOAT;
        else if (cgltf_component_type_r_32u == compType)
            return hz::Format::R32G32B32A32_UINT;
        // #NOTE: Not applicable to vertex formats
    case cgltf_type_mat2:
    case cgltf_type_mat3:
    case cgltf_type_mat4:
    default:
        return hz::Format::UNDEFINED;
    }
}

typedef void (*PackingFunction)(uint32_t Count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst);

static inline void util_pack_float2_to_half2(uint32_t Count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src,
                                             uint8_t* dst)
{
    COMPILE_ASSERT(sizeof(float2) == sizeof(float[2]));
    ASSERT(srcStride == sizeof(float2));
    ASSERT(dstStride == sizeof(uint32_t));

    float2* f = (float2*)src;
    for (uint32_t e = 0; e < Count; ++e)
    {
        *(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = packFloat2ToHalf2(f[e]);
    }
}

static inline void util_pack_float3_direction_to_half2(uint32_t Count, uint32_t srcStride, uint32_t dstStride, uint32_t offset,
                                                       const uint8_t* src, uint8_t* dst)
{
    COMPILE_ASSERT(sizeof(float3) == sizeof(float[3]));
    ASSERT(dstStride == sizeof(uint32_t));
    for (uint32_t e = 0; e < Count; ++e)
    {
        float3 f = *(float3*)(src + e * srcStride);
        *(uint32_t*)(dst + e * dstStride + offset) = packFloat3DirectionToHalf2(f);
    }
}

static inline void util_unpack_uint8_to_uint16_joints(uint32_t Count, uint32_t srcStride, uint32_t dstStride, uint32_t offset,
                                                      const uint8_t* src, uint8_t* dst)
{
    ASSERT(srcStride == 4 && "Expecting stride of 4 (sizeof(uint8_t) * 4 joints)");

    for (uint32_t e = 0; e < Count; ++e)
    {
        const uint8_t* srcBase = (const uint8_t*)(src + e * srcStride);
        uint16_t*      dstBase = (uint16_t*)(dst + e * dstStride + offset);
        dstBase[0] = srcBase[0];
        dstBase[1] = srcBase[1];
        dstBase[2] = srcBase[2];
        dstBase[3] = srcBase[3];
    }
}

void OnGLTFFind(ResourceDirectory resourceDir, const char* filename, void* UserData)
{
    UNREF_PARAM(resourceDir);
    bstring** inputFileNames = (bstring**)UserData;
    arrpush(*inputFileNames, bdynfromcstr(filename));
}

void buildMeshlets(const uint* indices, size_t indexCount, const float3* vertexPositions, size_t vertexCount, size_t vertexPositionsStride,
                   size_t maxVertices, size_t maxTriangles, float coneWeight, uint** meshletVertices, uint8_t** meshletTriangles,
                   Meshlet** meshlets, MeshletData** meshletsData)
{
    uint64_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, maxVertices, maxTriangles);

    meshopt_Meshlet* meshletsTmp = NULL;
    arrsetlen(meshletsTmp, maxMeshlets);

    MeshletData* meshletsDataTmp = NULL;
    arrsetlen(meshletsDataTmp, maxMeshlets);

    uint* meshletVerticesTmp = NULL;
    arrsetlen(meshletVerticesTmp, maxMeshlets * maxVertices);

    uint8_t* meshletTrianglesTmp = NULL;
    arrsetlen(meshletTrianglesTmp, maxMeshlets * maxTriangles * 3);

    uint64_t meshletCount =
        meshopt_buildMeshlets(meshletsTmp, meshletVerticesTmp, meshletTrianglesTmp, indices, indexCount, (float*)vertexPositions,
                              vertexCount, vertexPositionsStride, maxVertices, maxTriangles, coneWeight);

    if (meshletsTmp && meshletsDataTmp && meshletVerticesTmp && meshletTrianglesTmp)
    {
        const meshopt_Meshlet& last = meshletsTmp[meshletCount - 1];

        arrsetlen(meshletVerticesTmp, last.vertex_offset + last.vertex_count);
        arrsetlen(meshletTrianglesTmp, last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
        arrsetlen(meshletsTmp, meshletCount);

        for (uint m = 0; m < arrlenu(meshletsTmp); m++)
        {
            meshopt_Meshlet* meshlet = &meshletsTmp[m];
            meshopt_Bounds   bounds =
                meshopt_computeMeshletBounds(&meshletVerticesTmp[meshlet->vertex_offset], &meshletTrianglesTmp[meshlet->triangle_offset],
                                             meshlet->triangle_count, (float*)vertexPositions, vertexCount, vertexPositionsStride);

            MeshletData* meshletData = &meshletsDataTmp[m];
            meshletData->center = float3(bounds.center[0], bounds.center[1], bounds.center[2]);
            meshletData->radius = bounds.radius;
            meshletData->coneApex = float3(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]);
            meshletData->coneAxis = float3(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);
            meshletData->coneCutoff = bounds.cone_cutoff;
        }

        *meshlets = (Meshlet*)meshletsTmp;
        *meshletsData = meshletsDataTmp;
        *meshletVertices = meshletVerticesTmp;
        *meshletTriangles = meshletTrianglesTmp;
    }
    else
    {
        LOGF(Error, "Failed to build meshlets");
    }
}

static void geomOptimize(GeometryData* geomData, MeshOptimizerFlags optimizationFlags, IndexType indexType, uint32_t indexOffset,
                         uint32_t indexCount, uint32_t vertexOffset, uint32_t* vertexCount)
{
    if (optimizationFlags == MESH_OPTIMIZATION_FLAG_OFF)
        return;

    size_t remapSize = (*vertexCount * sizeof(uint32_t));

    uint32_t* remap = (uint32_t*)MemAllocate(remapSize);

    meshopt_Stream streams[MAX_SEMANTICS];
    uint32_t       validStreamCount = 0;
    int32_t        posAttributeIdx = -1;
    for (size_t i = 0; i < MAX_SEMANTICS; i++)
    {
        if (!geomData->pShadow->vertexStrides[i])
            continue;

        if (i == SEMANTIC_POSITION)
        {
            posAttributeIdx = (int32_t)i;
        }

        streams[validStreamCount].data = (uint8_t*)geomData->pShadow->pAttributes[i] + vertexOffset * geomData->pShadow->vertexStrides[i];
        streams[validStreamCount].size = geomData->pShadow->vertexStrides[i];
        streams[validStreamCount].stride = geomData->pShadow->vertexStrides[i];

        validStreamCount++;
    }

    // Remap functions expect the same vertex Count passed when generating the map, so we have to Set the new vertex Count later on
    uint32_t newVertCount = 0;

    // generating remap & new vertex/index sets
    if (indexType == IndexTypeUInt16)
    {
        newVertCount = (uint32_t)meshopt_generateVertexRemapMulti(remap, (uint16_t*)geomData->pShadow->pIndices + indexOffset, indexCount,
                                                                  *vertexCount, streams, validStreamCount);
        meshopt_remapIndexBuffer((uint16_t*)geomData->pShadow->pIndices + indexOffset, (uint16_t*)geomData->pShadow->pIndices + indexOffset,
                                 indexCount, remap);
    }
    else
    {
        newVertCount = (uint32_t)meshopt_generateVertexRemapMulti(remap, (uint32_t*)geomData->pShadow->pIndices + indexOffset, indexCount,
                                                                  *vertexCount, streams, validStreamCount);
        meshopt_remapIndexBuffer((uint32_t*)geomData->pShadow->pIndices + indexOffset, (uint32_t*)geomData->pShadow->pIndices + indexOffset,
                                 indexCount, remap);
    }

    for (size_t i = 0; i < MAX_SEMANTICS; i++)
    {
        if (!geomData->pShadow->vertexStrides[i])
            continue;

        uint8_t* buffer = (uint8_t*)geomData->pShadow->pAttributes[i] + vertexOffset * geomData->pShadow->vertexStrides[i];
        meshopt_remapVertexBuffer(buffer, buffer, *vertexCount, geomData->pShadow->vertexStrides[i], remap);
    }

    *vertexCount = newVertCount;

    // optimize
    // do we need to optmize per primitive ?
    /// optmizations like overdraw clearly can alter across primitives. but can vertex cache & vertex fetch do it ?
    if (indexType == IndexTypeUInt16)
    {
        if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXCACHE)
        {
            uint16_t* src = (uint16_t*)geomData->pShadow->pIndices + indexOffset;
            meshopt_optimizeVertexCache(src, src, indexCount, *vertexCount);
        }
        // we can only run this if position data is not packed
        if ((optimizationFlags & MESH_OPTIMIZATION_FLAG_OVERDRAW) && posAttributeIdx >= 0)
        {
            uint16_t* src = (uint16_t*)geomData->pShadow->pIndices + indexOffset;
            float*    pos = (float*)((uint8_t*)geomData->pShadow->pAttributes[posAttributeIdx] +
                                  vertexOffset * geomData->pShadow->vertexStrides[posAttributeIdx]);

            const float kThreshold = 1.01f;
            meshopt_optimizeOverdraw(src, src, indexCount, pos, *vertexCount, geomData->pShadow->vertexStrides[posAttributeIdx],
                                     kThreshold);
        }

        if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXFETCH)
        {
            uint16_t* indices = (uint16_t*)geomData->pShadow->pIndices + indexOffset;
            newVertCount = (uint32_t)meshopt_optimizeVertexFetchRemap(remap, indices, indexCount, *vertexCount);
            meshopt_remapIndexBuffer(indices, indices, indexCount, remap);

            for (size_t i = 0; i < MAX_SEMANTICS; i++)
            {
                if (!geomData->pShadow->vertexStrides[i])
                    continue;

                uint8_t* buffer = (uint8_t*)geomData->pShadow->pAttributes[i] + vertexOffset * geomData->pShadow->vertexStrides[i];
                meshopt_remapVertexBuffer(buffer, buffer, *vertexCount, geomData->pShadow->vertexStrides[i], remap);
            }
        }
    }
    else
    {
        if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXCACHE)
        {
            uint32_t* src = (uint32_t*)geomData->pShadow->pIndices + indexOffset;
            meshopt_optimizeVertexCache(src, src, indexCount, *vertexCount);
        }

        // we can only run this if position data is not packed
        if ((optimizationFlags & MESH_OPTIMIZATION_FLAG_OVERDRAW) && posAttributeIdx >= 0)
        {
            uint32_t* src = (uint32_t*)geomData->pShadow->pIndices + indexOffset;
            float*    pos = (float*)((uint8_t*)geomData->pShadow->pAttributes[posAttributeIdx] +
                                  vertexOffset * geomData->pShadow->vertexStrides[posAttributeIdx]);

            const float kThreshold = 1.01f;
            meshopt_optimizeOverdraw(src, src, indexCount, pos, *vertexCount, geomData->pShadow->vertexStrides[posAttributeIdx],
                                     kThreshold);
        }

        if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXFETCH)
        {
            uint32_t* indices = (uint32_t*)geomData->pShadow->pIndices + indexOffset;
            newVertCount = (uint32_t)meshopt_optimizeVertexFetchRemap(remap, indices, indexCount, *vertexCount);
            meshopt_remapIndexBuffer(indices, indices, indexCount, remap);

            for (size_t i = 0; i < MAX_SEMANTICS; i++)
            {
                if (!geomData->pShadow->vertexStrides[i])
                    continue;

                uint8_t* buffer = (uint8_t*)geomData->pShadow->pAttributes[i] + vertexOffset * geomData->pShadow->vertexStrides[i];
                meshopt_remapVertexBuffer(buffer, buffer, *vertexCount, geomData->pShadow->vertexStrides[i], remap);
            }
        }
    }

    *vertexCount = newVertCount;
    MemDeallocate(remap);
}

bool ProcessGLTF(AssetPipelineParams* assetParams, ProcessGLTFParams* glTFParams)
{
    VertexLayout* pVertexLayout = glTFParams->pVertexLayout;
    ASSERT(pVertexLayout);

    bool error = false;

    // Get all gltf files
    bstring* gltfFiles = NULL;
    uint32_t gltfFileCount = 0;

    if (assetParams->mPathMode == PROCESS_MODE_FILE)
    {
        arrpush(gltfFiles, bdynfromcstr(assetParams->mInFilePath));
    }
    else
    {
        DirectorySearch(assetParams->mRDInput, NULL, "gltf", OnGLTFFind, (void*)&gltfFiles,
                        assetParams->mPathMode == PROCESS_MODE_DIRECTORY_RECURSIVE);
    }

    gltfFileCount = (uint32_t)arrlenu(gltfFiles);

    for (uint32_t i = 0; i < gltfFileCount; ++i)
    {
        const char* fileName = (char*)gltfFiles[i].data;

        char newFileName[FS_MAX_PATH] = { 0 };
        if (assetParams->mOutSubdir)
        {
            char extractedFileName[FS_MAX_PATH] = {};
            GetPathFileName(fileName, extractedFileName);

            strcat(extractedFileName, ".bin");
            AppendPathComponent(assetParams->mOutSubdir, extractedFileName, newFileName);
        }
        else
        {
            ReplacePathExtension(fileName, "bin", newFileName);
        }

        LOGF(Info, "Converting %s to TF custom binary file", fileName);

        FileStream file = {};
        if (!OpenStreamFromPath(assetParams->mRDInput, fileName, FileModeRead, &file))
        {
            LOGF(Error, "Failed to open gltf file %s", fileName);
            error = true;
            continue;
        }

        const ssize_t fileSize = GetStreamFileSize(&file);
        if (fileSize <= 0)
        {
            LOGF(Error, "Failed to read gltf file %s", fileName);
            CloseStream(&file);
            error = true;
            continue;
        }
        void*      fileData = MemAllocate(fileSize);
        const bool fileRead = ReadFromStream(&file, fileData, fileSize) == (size_t)fileSize;

        cgltf_options options = {};
        cgltf_data*   data = NULL;
        options.memory_alloc = [](void* user, cgltf_size Size)
        {
            UNREF_PARAM(user);
            return MemAllocate(Size);
        };
        options.memory_free = [](void* user, void* ptr)
        {
            UNREF_PARAM(user);
            MemDeallocate(ptr);
        };
        options.rd = assetParams->mRDInput;
        cgltf_result result = fileRead ? cgltf_parse(&options, fileData, fileSize, &data) : cgltf_result_io_error;
        CloseStream(&file);

        if (cgltf_result_success != result)
        {
            LOGF(Error, "Failed to Parse gltf file %s with error %u", fileName, (uint32_t)result);
            MemDeallocate(fileData);
            error = true;
            continue;
        }

        result = cgltf_validate(data);
        if (cgltf_result_success != result)
        {
            LOGF(Error, "Invalid glTF file %s (error %u)", fileName, (uint32_t)result);
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        if (!validateSceneAssetCookInput(data))
        {
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        // Load buffers located in separate files (.bin) using our file system
        bool buffersLoaded = true;
        for (uint32_t j = 0; j < data->buffers_count; ++j)
        {
            const char* uri = data->buffers[j].uri;

            if (!uri || data->buffers[j].data)
            {
                continue;
            }

            if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
            {
                char parent[FS_MAX_PATH] = { 0 };
                GetParentPath(fileName, parent);
                char path[FS_MAX_PATH] = { 0 };
                AppendPathComponent(parent, uri, path);
                FileStream fs = {};
                if (OpenStreamFromPath(assetParams->mRDInput, path, FileModeRead, &fs))
                {
                    buffersLoaded = GetStreamFileSize(&fs) >= (ssize_t)data->buffers[j].size;
                    if (buffersLoaded)
                    {
                        data->buffers[j].data = MemAllocate(data->buffers[j].size);
                        buffersLoaded = ReadFromStream(&fs, data->buffers[j].data, data->buffers[j].size) == data->buffers[j].size;
                    }
                    CloseStream(&fs);
                    if (!buffersLoaded)
                        break;
                }
            }
        }

        result = buffersLoaded ? cgltf_load_buffers(&options, data, fileName) : cgltf_result_data_too_short;
        if (result == cgltf_result_success)
            result = cgltf_validate(data);
        if (cgltf_result_success != result)
        {
            LOGF(Error, "Failed to Load buffers from gltf file %s with error %u", fileName, (uint32_t)result);
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        SceneAssetCookFingerprint* pFingerprint = (SceneAssetCookFingerprint*)MemCallocate(1, sizeof(SceneAssetCookFingerprint));
        if (!pFingerprint || !buildSceneAssetCookFingerprint(assetParams->mRDInput, fileName, fileData, (size_t)fileSize, data, glTFParams,
                                                             assetParams->mAdditionalModifiedTime, pFingerprint))
        {
            LOGF(Error, "Failed to fingerprint glTF dependencies for %s", fileName);
            MemDeallocate(pFingerprint);
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        char contentHash[kSceneAssetCookHashCapacity] = {};
        formatSceneAssetCookHash(pFingerprint, contentHash);
        SceneAssetCooker sceneCooker(assetParams->mRDInput, fileName, assetParams->mRDOutput, newFileName);
        if (!assetParams->mSettings.force && FileExists(assetParams->mRDOutput, newFileName) && sceneCooker.isCurrent(contentHash))
        {
            LOGF(Info, "Skipping %s (content Hash unchanged)", fileName);
            MemDeallocate(pFingerprint);
            data->file_data = fileData;
            cgltf_free(data);
            continue;
        }

        if (!sceneCooker.prepare(*data) || !sceneCooker.build(*data, contentHash))
        {
            MemDeallocate(pFingerprint);
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        cgltf_attribute* vertexAttribs[MAX_SEMANTICS] = {};

        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
        uint32_t drawCount = 0;
        uint32_t jointCount = 0;
        uint32_t vertexAttribCount[MAX_SEMANTICS] = {};

        // Find number of traditional Draw calls required to Draw this piece of geometry
        // Find total index Count, total vertex Count
        for (uint32_t j = 0; j < data->meshes_count; ++j)
        {
            const cgltf_mesh* mesh = &data->meshes[j];
            const uint32_t    meshExtrasLength = (uint32_t)(mesh->extras.end_offset - mesh->extras.start_offset);
            if (glTFParams->pReadExtrasCallback && meshExtrasLength)
            {
                glTFParams->pReadExtrasCallback(j, (uint32_t)data->meshes_count, data->json + mesh->extras.start_offset, meshExtrasLength,
                                                glTFParams->pCallbackUserData);
            }

            for (uint32_t p = 0; p < mesh->primitives_count; ++p)
            {
                const cgltf_primitive* prim = &mesh->primitives[p];
                indexCount += (uint32_t)(prim->indices ? prim->indices->count : prim->attributes[0].data->count);
                vertexCount += (uint32_t)(prim->attributes[0].data->count);
                ++drawCount;

                for (uint32_t k = 0; k < prim->attributes_count; ++k)
                {
                    const uint32_t semanticIdx =
                        (uint32_t)util_cgltf_attrib_type_to_semantic(prim->attributes[k].type, prim->attributes[k].index);
                    if (semanticIdx == SEMANTIC_UNDEFINED)
                    {
                        continue;
                    }
                    vertexAttribs[semanticIdx] = &prim->attributes[k];
                    vertexAttribCount[semanticIdx] += (uint32_t)prim->attributes[k].data->count;
                }
            }
        }

        // Request the Size that the user will need to Load it's custom fields
        uint32_t userDataSize = 0;
        if (glTFParams->pWriteExtrasCallback)
            glTFParams->pWriteExtrasCallback(&userDataSize, NULL, glTFParams->pCallbackUserData);

        PackingFunction vertexPacking[MAX_SEMANTICS] = {};
        uint32_t        vertexAttrStrides[MAX_SEMANTICS] = {};

        for (uint32_t j = 0; j < data->skins_count; ++j)
            jointCount += (uint32_t)data->skins[j].joints_count;

        // Determine index stride
        // This depends on vertex Count rather than the stride specified in gltf
        // since gltf assumes we have index buffer per primitive which is non optimal
        const uint32_t indexStride =
            !glTFParams->mProcessMeshlets ? (vertexCount > UINT16_MAX ? sizeof(uint32_t) : sizeof(uint16_t)) : sizeof(uint32_t);

        uint32_t totalGeomSize = 0;
        totalGeomSize += round_up(sizeof(Geometry), 16);
        totalGeomSize += round_up(drawCount * sizeof(IndirectDrawIndexArguments), 16);

        uint32_t totalGeomDataSize = 0;
        totalGeomDataSize += round_up(sizeof(GeometryData), 16);
        totalGeomDataSize += round_up(jointCount * sizeof(Matrix4), 16);
        totalGeomDataSize += round_up(jointCount * sizeof(uint32_t), 16);
        totalGeomDataSize += round_up(userDataSize, 16);

        Geometry* geom = (Geometry*)MemCallocate(1, totalGeomSize);
        ASSERT(geom);

        GeometryData* geomData = (GeometryData*)MemCallocate(1, totalGeomDataSize);
        ASSERT(geomData);

        geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1); //-V1027

        if (jointCount > 0)
        {
            geomData->pInverseBindPoses = (Matrix4*)(geomData + 1); // -V1027
            geomData->pJointRemaps =
                (uint32_t*)((uint8_t*)geomData->pInverseBindPoses + round_up(jointCount * sizeof(*geomData->pInverseBindPoses), 16));
        }

        if (userDataSize > 0)
        {
            uint8_t* UserData = jointCount > 0 ? ((uint8_t*)geomData->pJointRemaps + round_up(jointCount * sizeof(uint32_t), 16))
                                               : (uint8_t*)(geomData + 1);
            geomData->UserData = UserData;
            geomData->userDataSize = userDataSize;
            glTFParams->pWriteExtrasCallback(&userDataSize, UserData, glTFParams->pCallbackUserData);
        }

        // Determine vertex stride for Each binding
        for (uint32_t attrIdx = 0; attrIdx < pVertexLayout->attribCount; ++attrIdx)
        {
            const VertexAttrib*    attr = &pVertexLayout->attribs[attrIdx];
            const cgltf_attribute* cgltfAttr = vertexAttribs[attr->semantic];

            if (!cgltfAttr)
            {
                if (glTFParams->mIgnoreMissingAttributes)
                    continue; // This attribute is not in the GLTF file, just ignore it
                else
                {
                    LOGF(Error, "Missing attribute at index %u of pVertexLayout", attrIdx);
                    error = true;
                    continue;
                }
            }

            const uint32_t dstFormatSize = TinyImageFormat_BitSizeOfBlock((TinyImageFormat)attr->Format) >> 3;
            const uint32_t srcFormatSize = (uint32_t)cgltfAttr->data->stride; //-V522

            const uint32_t thisAttrStride = dstFormatSize ? dstFormatSize : srcFormatSize;
            ASSERT(vertexAttrStrides[attr->semantic] == 0);
            vertexAttrStrides[attr->semantic] = thisAttrStride;

            // Compare vertex attrib Format to the gltf attrib Type
            // Select a packing function if dst Format is packed version
            // Texcoords - Pack float2 to half2
            // Directions - Pack float3 to float2 to unorm2x16 (Normal, Tangent)
            // Position - No packing yet
            const hz::Format srcFormat = util_cgltf_type_to_image_format(cgltfAttr->data->type, cgltfAttr->data->component_type);
            const hz::Format dstFormat = attr->Format == hz::Format::UNDEFINED ? srcFormat : attr->Format;

            if (dstFormat != srcFormat)
            {
                // Select appropriate packing function which will be used when filling the vertex buffer
                switch (cgltfAttr->type)
                {
                case cgltf_attribute_type_texcoord:
                {
                    if (sizeof(uint32_t) == dstFormatSize && sizeof(float[2]) == srcFormatSize)
                        vertexPacking[attr->semantic] = util_pack_float2_to_half2;
                    // #TODO: Add more variations if needed
                    break;
                }
                case cgltf_attribute_type_normal:
                case cgltf_attribute_type_tangent:
                {
                    if (sizeof(uint32_t) == dstFormatSize && (sizeof(float[3]) == srcFormatSize || sizeof(float[4]) == srcFormatSize))
                        vertexPacking[attr->semantic] = util_pack_float3_direction_to_half2;
                    // #TODO: Add more variations if needed
                    break;
                }
                case cgltf_attribute_type_joints:
                {
                    if (srcFormatSize == sizeof(uint8_t) * 4 && dstFormatSize == sizeof(uint16_t) * 4)
                        vertexPacking[attr->semantic] = util_unpack_uint8_to_uint16_joints;
                    else
                    {
                        LOGF(Error, "Joint size doesn't match in '%s'", fileName);
                        error = true;
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }

        uint32_t shadowSize = 0;
        shadowSize += sizeof(GeometryData::ShadowData);
        shadowSize += indexCount * indexStride;

        for (uint32_t s = 0; s < MAX_SEMANTICS; ++s)
        {
            // Streams use global vertex offsets, including vertices whose primitive omits this attribute.
            vertexAttribCount[s] = vertexAttrStrides[s] ? vertexCount : 0;

            shadowSize += vertexAttrStrides[s] * vertexAttribCount[s];
        }

        geomData->pShadow = (GeometryData::ShadowData*)MemCallocate(1, shadowSize);
        geomData->pShadow->pIndices = geomData->pShadow + 1;

        // Same strides as the ones used in the GPU
        for (uint32_t s = 0; s < MAX_SEMANTICS; ++s)
        {
            geomData->pShadow->vertexStrides[s] = vertexAttrStrides[s];
            geomData->pShadow->attributeCount[s] = vertexAttribCount[s];
        }

        geomData->pShadow->pAttributes[SEMANTIC_POSITION] = (uint8_t*)geomData->pShadow->pIndices + (indexCount * indexStride);

        for (uint32_t s = SEMANTIC_POSITION + 1; s < MAX_SEMANTICS; ++s)
            geomData->pShadow->pAttributes[s] = (uint8_t*)geomData->pShadow->pAttributes[s - 1] +
                                                geomData->pShadow->vertexStrides[s - 1] * geomData->pShadow->attributeCount[s - 1];

        ASSERT(((const char*)geomData->pShadow) + shadowSize ==
               ((char*)geomData->pShadow->pAttributes[MAX_SEMANTICS - 1] +
                geomData->pShadow->vertexStrides[MAX_SEMANTICS - 1] * geomData->pShadow->attributeCount[MAX_SEMANTICS - 1]));

        COMPILE_ASSERT(TF_ARRAY_COUNT(geomData->pShadow->vertexStrides) == TF_ARRAY_COUNT(geomData->pShadow->pAttributes));
        for (uint32_t j = 1; j < TF_ARRAY_COUNT(geomData->pShadow->vertexStrides); ++j)
        {
            // If the attribute is not present in the gltf file we just don't Save anything
            if (geomData->pShadow->vertexStrides[j] == 0)
                geomData->pShadow->pAttributes[j] = nullptr;
        }

        geom->drawArgCount = drawCount;
        geom->indexType = (sizeof(uint16_t) == indexStride) ? IndexTypeUInt16 : IndexTypeUInt32;
        geomData->jointCount = jointCount;

        indexCount = 0;
        vertexCount = 0;
        drawCount = 0;

        // Load the remap joint indices generated in the offline process
        uint32_t remapCount = 0;
        for (uint32_t j = 0; j < data->skins_count; ++j)
        {
            const cgltf_skin* skin = &data->skins[j];
            uint32_t          extrasSize = (uint32_t)(skin->extras.end_offset - skin->extras.start_offset);
            if (extrasSize)
            {
                const char* jointRemaps = (const char*)data->json + skin->extras.start_offset;
                jsmn_parser parser = {};
                jsmntok_t*  tokens = (jsmntok_t*)MemAllocate((skin->joints_count + 1) * sizeof(jsmntok_t));
                const int   tokenCount = jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
                // The extras are an array of one remap per joint.
                if (tokenCount != (int)skin->joints_count + 1 || tokens[0].type != JSMN_ARRAY || tokens[0].size != (int)skin->joints_count)
                {
                    LOGF(Error, "Skin %u in '%s' needs one joint remap per joint", j, fileName);
                    MemDeallocate(tokens);
                    error = true;
                    break;
                }
                cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, (cgltf_float*)geomData->pInverseBindPoses,
                                             skin->joints_count * sizeof(float[16]) / sizeof(float));
                for (uint32_t r = 0; r < skin->joints_count; ++r)
                    geomData->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
                MemDeallocate(tokens);
            }

            remapCount += (uint32_t)skin->joints_count;
        }

        if (glTFParams->mOptimizationFlags != MESH_OPTIMIZATION_FLAG_OFF)
        {
            for (uint32_t s = 0; s < MAX_SEMANTICS; ++s)
            {
                geomData->pShadow->attributeCount[s] = 0;
            }
        }

        for (uint32_t j = 0; j < data->meshes_count; ++j)
        {
            for (uint32_t p = 0; p < data->meshes[j].primitives_count; ++p)
            {
                const cgltf_primitive* prim = &data->meshes[j].primitives[p];
                const uint32_t primitiveIndexCount = (uint32_t)(prim->indices ? prim->indices->count : prim->attributes[0].data->count);

                /************************************************************************/
                // Fill index buffer for this primitive
                /************************************************************************/
                if (sizeof(uint16_t) == indexStride)
                {
                    uint16_t* dst = (uint16_t*)geomData->pShadow->pIndices;
                    for (uint32_t idx = 0; idx < primitiveIndexCount; ++idx)
                        dst[indexCount + idx] = (uint16_t)(prim->indices ? cgltf_accessor_read_index(prim->indices, idx) : idx);
                }
                else
                {
                    uint32_t* dst = (uint32_t*)geomData->pShadow->pIndices;
                    for (uint32_t idx = 0; idx < primitiveIndexCount; ++idx)
                        dst[indexCount + idx] = (uint32_t)(prim->indices ? cgltf_accessor_read_index(prim->indices, idx) : idx);
                }

                /************************************************************************/
                // Fill vertex buffers for this primitive
                /************************************************************************/
                cgltf_attribute* pos_attr = NULL;
                for (uint32_t a = 0; a < prim->attributes_count; ++a)
                {
                    cgltf_attribute* attr = &prim->attributes[a];
                    const uint32_t   semanticIdx = (uint32_t)util_cgltf_attrib_type_to_semantic(attr->type, attr->index);
                    if (semanticIdx == SEMANTIC_UNDEFINED)
                    {
                        continue;
                    }

                    if (semanticIdx == SEMANTIC_POSITION)
                        pos_attr = attr;

                    // TODO: this should probably be an ASSERT, we don't want to loose data when using pShadow
                    if (geomData->pShadow->vertexStrides[semanticIdx] != 0)
                    {
                        const uint32_t stride = geomData->pShadow->vertexStrides[semanticIdx];

                        const uint8_t* src =
                            (uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
                        uint8_t* dst = (uint8_t*)geomData->pShadow->pAttributes[semanticIdx] + (uint64_t)vertexCount * stride;

                        // For now we just copy attributes to it's own buffer, in case of interleaved attributes we pack them in the
                        // ResourceLoader during Load
                        // TODO: Consider adding an option to make the attributes interleaved here in the AssetPipeline so that we don't
                        // have to do it in runtime.
                        //       The inconvenience in that case would be that in order to Get the pShadow data we would have to unpack the
                        //       interleaved attributes, another option would be to store interleaved buffers AND pShadow buffers (duplicate
                        //       some data). If we duplicate data would be better to only store pShadow data for the meshes that require it
                        //       rather than all of them, that would require more configuration parameters per mesh while running the
                        //       AssetPipeline.
                        if (vertexPacking[semanticIdx])
                            vertexPacking[semanticIdx]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, stride, 0, src, dst);
                        else
                            memcpy(dst, src, attr->data->count * attr->data->stride);
                    }
                }

                /************************************************************************/
                // Optimize mesh
                /************************************************************************/
                uint32_t optimizedVertexCount = (uint32_t)prim->attributes[0].data->count;
                if (glTFParams->mOptimizationFlags != MESH_OPTIMIZATION_FLAG_OFF)
                {
                    geomOptimize(geomData, MESH_OPTIMIZATION_FLAG_ALL, (IndexType)geom->indexType, indexCount, primitiveIndexCount,
                                 vertexCount, &optimizedVertexCount);

                    for (uint32_t s = 0; s < MAX_SEMANTICS; ++s)
                    {
                        if (geomData->pShadow->vertexStrides[s] > 0)
                        {
                            geomData->pShadow->attributeCount[s] += optimizedVertexCount;
                        }
                    }
                }

                /************************************************************************/
                // Fill Draw arguments for this primitive
                /************************************************************************/
                geom->pDrawArgs[drawCount].indexCount = primitiveIndexCount;
                geom->pDrawArgs[drawCount].instanceCount = 1;
                geom->pDrawArgs[drawCount].startIndex = indexCount;
                geom->pDrawArgs[drawCount].startInstance = 0;
                // Since we already offset indices when creating the index buffer, vertex offset will be zero
                // With this approach, we can Draw everything in one Draw call or use the traditional Draw per subset without the
                // need for changing shader code
                geom->pDrawArgs[drawCount].vertexOffset = 0;

                if (glTFParams->mProcessMeshlets)
                {
                    if (indexStride != 4)
                    {
                        LOGF(Error, "Cannot create meshlet when index type isn't 32-bit.");
                        error = true;
                        break;
                    }

                    if (!pos_attr || pos_attr->data->stride != 12)
                    {
                        LOGF(Error, "Cannot create meshlet when positions attribute is missing or layout is not float3.");
                        error = true;
                        break;
                    }

                    float3* positions =
                        (float3*)((uint8_t*)geomData->pShadow->pAttributes[SEMANTIC_POSITION] + vertexCount * pos_attr->data->stride);

                    const uint64_t maxVertices = (uint64_t)glTFParams->mNumMaxVertices;
                    const uint64_t maxTriangles = (uint64_t)glTFParams->mNumMaxTriangles;
                    // 0.0 had better results overall
                    const float    coneWeight = 0.0f;

                    Meshlet*     subMeshlets = NULL;
                    MeshletData* meshletsData = NULL;
                    // indices for positions
                    uint*        meshletVertices = NULL;
                    uint8_t*     meshletTriangles = NULL;

                    buildMeshlets((uint32_t*)geomData->pShadow->pIndices + indexCount, primitiveIndexCount, positions,
                                  pos_attr->data->count, pos_attr->data->stride, maxVertices, maxTriangles, coneWeight, &meshletVertices,
                                  &meshletTriangles, &subMeshlets, &meshletsData);

                    arrsetlen(geom->meshlets.vertices, geom->meshlets.vertexCount + arrlenu(meshletVertices));
                    for (uint64_t index_id = 0; index_id < arrlenu(meshletVertices); ++index_id)
                    {
                        geom->meshlets.vertices[geom->meshlets.vertexCount + index_id] = meshletVertices[index_id] + vertexCount;
                    }

                    arrsetlen(geom->meshlets.triangles, geom->meshlets.triangleCount + arrlenu(meshletTriangles));
                    memcpy(geom->meshlets.triangles + geom->meshlets.triangleCount, meshletTriangles,
                           arrlenu(meshletTriangles) * sizeof *meshletTriangles);

                    arrsetlen(geom->meshlets.meshletsData, geom->meshlets.meshletCount + arrlenu(subMeshlets));
                    memcpy((void*)(geom->meshlets.meshletsData + geom->meshlets.meshletCount), meshletsData, //-V595
                           arrlenu(subMeshlets) * sizeof *meshletsData);

                    arrsetlen(geom->meshlets.meshlets, geom->meshlets.meshletCount + arrlenu(subMeshlets));
                    memcpy(geom->meshlets.meshlets + geom->meshlets.meshletCount, subMeshlets, arrlenu(subMeshlets) * sizeof *subMeshlets);

                    for (uint64_t meshlet_id = 0; meshlet_id < arrlenu(subMeshlets); ++meshlet_id)
                    {
                        geom->meshlets.meshlets[geom->meshlets.meshletCount + meshlet_id].triangleOffset +=
                            (uint)geom->meshlets.triangleCount;
                        geom->meshlets.meshlets[geom->meshlets.meshletCount + meshlet_id].vertexOffset += (uint)geom->meshlets.vertexCount;
                    }

                    geom->meshlets.vertexCount += arrlenu(meshletVertices);
                    geom->meshlets.triangleCount += arrlenu(meshletTriangles);
                    geom->meshlets.meshletCount += arrlenu(subMeshlets);

                    arrfree(subMeshlets);
                    arrfree(meshletsData);
                    arrfree(meshletVertices);
                    arrfree(meshletTriangles);
                }

                if (sizeof(uint16_t) == indexStride)
                {
                    for (uint32_t idx = 0; idx < primitiveIndexCount; ++idx)
                        ((uint16_t*)geomData->pShadow->pIndices)[indexCount + idx] += (uint16_t)vertexCount;
                }
                else
                {
                    for (uint32_t idx = 0; idx < primitiveIndexCount; ++idx)
                        ((uint32_t*)geomData->pShadow->pIndices)[indexCount + idx] += vertexCount;
                }

                indexCount += primitiveIndexCount;
                vertexCount += optimizedVertexCount;
                ++drawCount;
            }
        }

        geom->indexCount = indexCount;
        geom->vertexCount = vertexCount;

        // Tighten the vertex attribute buffers
        if (glTFParams->mOptimizationFlags != MESH_OPTIMIZATION_FLAG_OFF)
        {
            size_t largestSizedAttribute = 0;
            for (uint32_t s = SEMANTIC_POSITION + 1; s < MAX_SEMANTICS; ++s)
            {
                largestSizedAttribute =
                    max(largestSizedAttribute, (size_t)geomData->pShadow->vertexStrides[s] * geomData->pShadow->attributeCount[s]);
            }
            void* tempStagingBuffer = MemAllocate(largestSizedAttribute);

            for (uint32_t s = SEMANTIC_POSITION + 1; s < MAX_SEMANTICS; ++s)
            {
                size_t Size = (size_t)geomData->pShadow->vertexStrides[s] * geomData->pShadow->attributeCount[s];
                if (Size > 0)
                {
                    memcpy(tempStagingBuffer, geomData->pShadow->pAttributes[s], Size);
                }

                geomData->pShadow->pAttributes[s] = (uint8_t*)geomData->pShadow->pAttributes[s - 1] +
                                                    geomData->pShadow->vertexStrides[s - 1] * geomData->pShadow->attributeCount[s - 1];

                if (Size > 0)
                {
                    memcpy(geomData->pShadow->pAttributes[s], tempStagingBuffer, Size);
                }
            }

            MemDeallocate(tempStagingBuffer);
        }

        for (uint32_t j = 0; j < TF_ARRAY_COUNT(geom->vertexStrides); ++j)
            ASSERT(geom->vertexStrides[j] == 0);

        CreateDirectoryForFile(assetParams->mRDOutput, newFileName);

        CookedFile geometryFile(assetParams->mRDOutput, newFileName);
        geometryFile.Write(GEOMETRY_FILE_MAGIC_STR, sizeof(GEOMETRY_FILE_MAGIC_STR));
        geometryFile.Write(&GEOMETRY_FILE_VERSION, sizeof(GEOMETRY_FILE_VERSION));
        geometryFile.Write(GEOMETRY_FILE_LAYOUT, sizeof(GEOMETRY_FILE_LAYOUT));
        geometryFile.Write(&totalGeomSize, sizeof(totalGeomSize));
        geometryFile.Write(geom, totalGeomSize);

        // Serialized pointers are restored from the following data blocks during Load.
        GeometryData::ShadowData* pTempShadow = geomData->pShadow;
        void*                     pTempUserData = geomData->UserData;
        geomData->pShadow = nullptr;
        geomData->UserData = nullptr;
        geometryFile.Write(&totalGeomDataSize, sizeof(totalGeomDataSize));
        geometryFile.Write(geomData, totalGeomDataSize);
        geomData->pShadow = pTempShadow;
        geomData->UserData = pTempUserData;
        geometryFile.Write(&shadowSize, sizeof(shadowSize));
        geometryFile.Write(geomData->pShadow, shadowSize);
        if (geom->meshlets.meshletCount)
        {
            geometryFile.Write(geom->meshlets.meshlets, sizeof(*geom->meshlets.meshlets) * geom->meshlets.meshletCount);
            geometryFile.Write(geom->meshlets.meshletsData, sizeof(*geom->meshlets.meshletsData) * geom->meshlets.meshletCount);
            geometryFile.Write(geom->meshlets.vertices, sizeof(*geom->meshlets.vertices) * geom->meshlets.vertexCount);
            geometryFile.Write(geom->meshlets.triangles, sizeof(*geom->meshlets.triangles) * geom->meshlets.triangleCount);
        }
        const bool geometryWritten = geometryFile.Publish();
        if (!geometryWritten)
        {
            LOGF(Error, "Failed to publish geometry '%s'", newFileName);
            error = true;
        }

        if (geometryWritten && !sceneCooker.write())
        {
            LOGF(Error, "Failed to write scene asset files for '%s'.", fileName);
            error = true;
        }

        MemDeallocate(geomData->pShadow);
        MemDeallocate(geomData);

        if (geom->meshlets.meshletCount)
        {
            arrfree(geom->meshlets.meshlets);
            arrfree(geom->meshlets.meshletsData);
            arrfree(geom->meshlets.vertices);
            arrfree(geom->meshlets.triangles);
        }

        MemDeallocate(geom);
        MemDeallocate(pFingerprint);

        data->file_data = fileData;
        cgltf_free(data);
    }

    if (gltfFiles)
    {
        for (uint32_t i = 0; i < gltfFileCount; ++i)
        {
            bdestroy(&gltfFiles[i]);
        }
        arrfree(gltfFiles);
    }

    return error;
}

int AssetPipelineRun(AssetPipelineParams* assetParams)
{
    if (assetParams->mProcessType == PROCESS_GLTF)
    {
        bool error = false;

        MeshOptimizerFlags meshOptimizerFlags = MESH_OPTIMIZATION_FLAG_OFF;

        bool processMeshlets = false;
        /// Recommended number of vertices and triangles are from
        /// https://gpuopen.com/learn/mesh_shaders/mesh_shaders-optimization_and_best_practices/
        int  numMeshletVertices = 128;
        int  numMeshletTriangles = 256;

        for (int32_t i = 0; i < assetParams->mFlagsCount; ++i)
        {
            if (strcmp(assetParams->mFlags[i], "--optimize") == 0)
            {
                meshOptimizerFlags |= MESH_OPTIMIZATION_FLAG_ALL;
            }
            else if (strcmp(assetParams->mFlags[i], "--optimizecache") == 0)
                meshOptimizerFlags |= MESH_OPTIMIZATION_FLAG_VERTEXCACHE;
            else if (strcmp(assetParams->mFlags[i], "--optimizeoverdraw") == 0)
                meshOptimizerFlags |= MESH_OPTIMIZATION_FLAG_OVERDRAW;
            else if (strcmp(assetParams->mFlags[i], "--optimizefetch") == 0)
                meshOptimizerFlags |= MESH_OPTIMIZATION_FLAG_VERTEXFETCH;
            else if (strcmp(assetParams->mFlags[i], "--meshlets") == 0)
                processMeshlets = true;
            else if (strcmp(assetParams->mFlags[i], "--meshletnumvertices") == 0)
            {
                i++;
                processMeshlets = true;
                numMeshletVertices = (uint32_t)atoi(assetParams->mFlags[i]);

                if (numMeshletVertices <= 0 || numMeshletVertices > 255)
                {
                    LOGF(Error, "Maximum number of vertices in a meshlet should be larger than 0 and smaller than 256.");
                    error = true;
                }
            }
            else if (strcmp(assetParams->mFlags[i], "--meshletnumtriangles") == 0)
            {
                i++;
                processMeshlets = true;
                numMeshletTriangles = (uint32_t)atoi(assetParams->mFlags[i]);

                if (numMeshletTriangles <= 0 || numMeshletTriangles > 512)
                {
                    LOGF(Error, "Maximum number of triangles in a meshlet should be larger than 0 and smaller than 513.");
                    error = true;
                }
            }
        }

        if (error)
            return 1;

        VertexLayout vertexLayout = {};

        vertexLayout.attribCount = 6;
        vertexLayout.attribs[0].semantic = SEMANTIC_POSITION;
        vertexLayout.attribs[0].Format = hz::Format::R32G32B32_SFLOAT;
        vertexLayout.attribs[0].binding = 0;
        vertexLayout.attribs[0].location = 0;
        vertexLayout.attribs[0].offset = 0;
        vertexLayout.attribs[1].semantic = SEMANTIC_NORMAL;
        vertexLayout.attribs[1].Format = hz::Format::R32_UINT;
        vertexLayout.attribs[1].binding = 1;
        vertexLayout.attribs[1].location = 1;
        vertexLayout.attribs[1].offset = 0;
        vertexLayout.attribs[2].semantic = SEMANTIC_TANGENT;
        vertexLayout.attribs[2].Format = hz::Format::R32_UINT;
        vertexLayout.attribs[2].binding = 2;
        vertexLayout.attribs[2].location = 2;
        vertexLayout.attribs[2].offset = 0;
        vertexLayout.attribs[3].semantic = SEMANTIC_TEXCOORD0;
        vertexLayout.attribs[3].Format = hz::Format::R32_UINT;
        vertexLayout.attribs[3].binding = 3;
        vertexLayout.attribs[3].location = 3;
        vertexLayout.attribs[3].offset = 0;
        vertexLayout.attribs[4].semantic = SEMANTIC_JOINTS;
        vertexLayout.attribs[4].Format = hz::Format::R16G16B16A16_UINT;
        vertexLayout.attribs[4].binding = 4;
        vertexLayout.attribs[4].location = 4;
        vertexLayout.attribs[4].offset = 0;
        vertexLayout.attribs[5].semantic = SEMANTIC_WEIGHTS;
        vertexLayout.attribs[5].Format = hz::Format::R32G32B32A32_SFLOAT;
        vertexLayout.attribs[5].binding = 5;
        vertexLayout.attribs[5].location = 5;
        vertexLayout.attribs[5].offset = 0;

        ProcessGLTFParams glTFParams = {};
        glTFParams.pVertexLayout = &vertexLayout;
        glTFParams.mIgnoreMissingAttributes = true;
        glTFParams.mProcessMeshlets = processMeshlets;
        glTFParams.mNumMaxVertices = numMeshletVertices;
        glTFParams.mNumMaxTriangles = numMeshletTriangles;
        glTFParams.mOptimizationFlags = meshOptimizerFlags;

        BeginAssetPipelineSection("ProcessGLTF");
        bool result = ProcessGLTF(assetParams, &glTFParams);
        EndAssetPipelineSection("ProcessGLTF");
        return result;
    }

    // if reached, the used processed Type wasn't included
    ASSERT(false);
    return 0;
}

bool ensureSceneGltfCooked(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory)
{
    if (!pSourceFile || !pSourceFile[0] || sourceDirectory == outputDirectory ||
        strcmp(GetResourceDirectory(sourceDirectory), GetResourceDirectory(outputDirectory)) == 0)
    {
        LOGF(Error, "glTF cooking requires separate source and output directories");
        return false;
    }
    AssetPipelineParams params = {};
    params.mInFilePath = pSourceFile;
    params.mInExt = "gltf";
    params.mProcessType = PROCESS_GLTF;
    params.mPathMode = PROCESS_MODE_FILE;
    params.mRDInput = sourceDirectory;
    params.mRDOutput = outputDirectory;
    if (!MakeDirectory(outputDirectory, "", true) || AssetPipelineRun(&params) != 0)
    {
        LOGF(Error, "Failed to cook %s", pSourceFile);
        return false;
    }
    return true;
}
