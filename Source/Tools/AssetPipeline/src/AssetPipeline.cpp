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

// Math
#include <ThirdParty/ModifiedSonyMath1/vectormath.hpp>

#include "Runtime/Core/Private/Math/ShaderUtilities.h"

// OZZ
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/animation_optimizer.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/track_optimizer.h>
#include <ozz/base/io/archive.h>
#include <ThirdParty/tinyimageformat/tinyimageformat_base.h>

// TressFX
#if defined(HORIZON_ENABLE_TRESSFX_COOKER)
#include "TressFXAsset.h"
#endif

// Meshoptimizer
#include <ThirdParty/meshoptimizer/src/meshoptimizer.h>

#ifdef ENABLE_ASSET_PIPELINE_CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#endif
#ifdef ENABLE_ASSET_PIPELINE_CGLTF_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#endif
// We are using jsmn to parse joints, these functions are static so it doesn't matter if they are defined in other files.
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

class CookedGeometryFile
{
public:
    CookedGeometryFile(ResourceDirectory directory, const char* pPath): directory(directory), pPath(pPath)
    {
        if (snprintf(temporary, sizeof(temporary), "%s.tmp", pPath) >= (int)sizeof(temporary) ||
            snprintf(backup, sizeof(backup), "%s.bak", pPath) >= (int)sizeof(backup))
            return;
        if (!fsFileExist(directory, pPath) && fsFileExist(directory, backup) && !fsRenameFile(directory, backup, pPath))
            return;
        opened = fsOpenStreamFromPath(directory, temporary, FM_WRITE_ALLOW_READ, &stream);
        written = opened;
    }
    ~CookedGeometryFile()
    {
        if (opened)
            fsCloseStream(&stream);
    }
    CookedGeometryFile(const CookedGeometryFile&) = delete;
    CookedGeometryFile& operator=(const CookedGeometryFile&) = delete;

    void write(const void* pData, size_t size)
    {
        if (written)
            written = fsWriteToStream(&stream, pData, size) == size;
    }
    bool publish()
    {
        if (!opened)
            return false;
        const bool closed = fsCloseStream(&stream);
        opened = false;
        if (!written || !closed)
            return false;
        const bool exists = fsFileExist(directory, pPath);
        if ((fsFileExist(directory, backup) && !fsRemoveFile(directory, backup)) || (exists && !fsRenameFile(directory, pPath, backup)))
            return false;
        if (!fsRenameFile(directory, temporary, pPath))
        {
            if (exists && !fsRenameFile(directory, backup, pPath))
                LOGF(eERROR, "Restore '%s' from '%s' before retrying", pPath, backup);
            return false;
        }
        if (exists && !fsRemoveFile(directory, backup))
            LOGF(eWARNING, "Cannot remove completed geometry backup '%s'", backup);
        return true;
    }

private:
    ResourceDirectory directory;
    const char*       pPath;
    FileStream        stream = {};
    char              temporary[FS_MAX_PATH] = {};
    char              backup[FS_MAX_PATH] = {};
    bool              opened = false;
    bool              written = false;
};

#include "Core/IMemory.h" //NOTE: this should be the last include in a .cpp

struct SkeletonNodeInfo
{
    bstring     mName = bempty();
    cgltf_node* pNode = nullptr;
    int32_t*    mChildNodeIndices = nullptr;
    int32_t     pParentNodeIndex = 0;
    bool        mUsedInSkeleton = false;
};

struct SkeletonBoneInfo
{
    int32_t                                      mNodeIndex;
    int32_t                                      mParentNodeIndex;
    ozz::animation::offline::RawSkeleton::Joint* pParentJoint;
};

// Private Func forward decls
bool CreateRuntimeSkeleton(ResourceDirectory resourceDirInput, const char* skeletonInputfile, ResourceDirectory resourceDirGltfOutput,
                           ResourceDirectory resourceDirOzzOutput, const char* skeletonOutputFile, ozz::animation::Skeleton* pOutSkeleton,
                           ProcessAssetsSettings* settings);

bool CreateRuntimeAnimations(ResourceDirectory resourceDirInput, const char* animationInputFile, const char* animationOutputPath,
                             ozz::animation::Skeleton* skeleton, RuntimeAnimationSettings* animationSettings,
                             ProcessAssetsSettings* settings);

// Used when discovering animations
struct OnDiscoverAnimationsParam
{
    SkeletonAndAnimations* pSkeletonAndAnimations = nullptr;
};

static void BeginAssetPipelineSection(const char* section) { LOGF(eINFO, "========== %s ==========", section); }

static void EndAssetPipelineSection(const char* section) { LOGF(eINFO, "========== !%s ==========", section); }

static bool validateSceneAssetCookInput(const cgltf_data* pData)
{
    for (cgltf_size i = 0; i < pData->meshes_count; ++i)
        for (cgltf_size j = 0; j < pData->meshes[i].primitives_count; ++j)
            if (pData->meshes[i].primitives[j].type != cgltf_primitive_type_triangles)
            {
                LOGF(eERROR, "Scene mesh %u primitive %u requires triangle-list topology", (uint32_t)i, (uint32_t)j);
                return false;
            }
    for (cgltf_size i = 0; i < pData->textures_count; ++i)
    {
        const cgltf_image* pImage = pData->textures[i].image;
        if (!pImage || (!pImage->uri && !pImage->buffer_view) || (pImage->uri && pImage->buffer_view))
        {
            LOGF(eERROR, "Scene texture %u must reference an image URI or buffer view", (uint32_t)i);
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

static void updateSceneAssetCookHash(uint64_t* pHash, const void* pData, size_t size)
{
    const uint8_t* pBytes = (const uint8_t*)pData;
    for (size_t i = 0; i < size; ++i)
    {
        *pHash ^= pBytes[i];
        *pHash *= UINT64_C(1099511628211);
    }
}

static bool addSceneAssetCookDependency(SceneAssetCookFingerprint* pFingerprint, const char* pPath, const void* pData, size_t size)
{
    char normalizedPath[FS_MAX_PATH] = {};
    fsNormalizePath(pPath, '/', normalizedPath);
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
    updateSceneAssetCookHash(&pFingerprint->mHash, normalizedPath, strlen(normalizedPath) + 1);
    updateSceneAssetCookHash(&pFingerprint->mHash, pData, size);
    return true;
}

static bool addSceneAssetCookFileDependency(ResourceDirectory resourceDirectory, SceneAssetCookFingerprint* pFingerprint, const char* pPath)
{
    FileStream stream = {};
    if (!fsOpenStreamFromPath(resourceDirectory, pPath, FM_READ, &stream))
        return false;

    char normalizedPath[FS_MAX_PATH] = {};
    fsNormalizePath(pPath, '/', normalizedPath);
    for (uint32_t i = 0; i < pFingerprint->mDependencyCount; ++i)
    {
        if (strcmp(pFingerprint->mDependencies[i], normalizedPath) == 0)
        {
            fsCloseStream(&stream);
            return true;
        }
    }
    if (!normalizedPath[0] || strlen(normalizedPath) >= FS_MAX_PATH || pFingerprint->mDependencyCount >= kSceneAssetCookMaxDependencies)
    {
        fsCloseStream(&stream);
        return false;
    }

    snprintf(pFingerprint->mDependencies[pFingerprint->mDependencyCount++], FS_MAX_PATH, "%s", normalizedPath);
    updateSceneAssetCookHash(&pFingerprint->mHash, normalizedPath, strlen(normalizedPath) + 1);
    uint8_t bytes[64 * 1024];
    for (;;)
    {
        const size_t bytesRead = fsReadFromStream(&stream, bytes, sizeof(bytes));
        if (!bytesRead)
            break;
        updateSceneAssetCookHash(&pFingerprint->mHash, bytes, bytesRead);
    }
    return fsCloseStream(&stream);
}

static bool buildSceneAssetCookFingerprint(ResourceDirectory resourceDirectory, const char* pSourcePath, const void* pSourceData,
                                           size_t sourceSize, const cgltf_data* pData, const ProcessGLTFParams* pParams,
                                           time_t additionalModifiedTime, SceneAssetCookFingerprint* pFingerprint)
{
    static const uint32_t kCookerRevision = 11;
    pFingerprint->mHash = UINT64_C(14695981039346656037);
    updateSceneAssetCookHash(&pFingerprint->mHash, &kCookerRevision, sizeof(kCookerRevision));
    updateSceneAssetCookHash(&pFingerprint->mHash, &additionalModifiedTime, sizeof(additionalModifiedTime));
    updateSceneAssetCookHash(&pFingerprint->mHash, pParams->pVertexLayout, sizeof(*pParams->pVertexLayout));
    updateSceneAssetCookHash(&pFingerprint->mHash, &pParams->mIgnoreMissingAttributes, sizeof(pParams->mIgnoreMissingAttributes));
    updateSceneAssetCookHash(&pFingerprint->mHash, &pParams->mProcessMeshlets, sizeof(pParams->mProcessMeshlets));
    updateSceneAssetCookHash(&pFingerprint->mHash, &pParams->mNumMaxVertices, sizeof(pParams->mNumMaxVertices));
    updateSceneAssetCookHash(&pFingerprint->mHash, &pParams->mNumMaxTriangles, sizeof(pParams->mNumMaxTriangles));
    updateSceneAssetCookHash(&pFingerprint->mHash, &pParams->mOptimizationFlags, sizeof(pParams->mOptimizationFlags));
    if (!addSceneAssetCookDependency(pFingerprint, pSourcePath, pSourceData, sourceSize))
        return false;

    char sourceParent[FS_MAX_PATH] = {};
    fsGetParentPath(pSourcePath, sourceParent);
    for (cgltf_size i = 0; i < pData->buffers_count; ++i)
    {
        const cgltf_buffer* pBuffer = &pData->buffers[i];
        if (!pBuffer->uri || strncmp(pBuffer->uri, "data:", 5) == 0 || strstr(pBuffer->uri, "://"))
            continue;
        char dependencyPath[FS_MAX_PATH] = {};
        fsAppendPathComponent(sourceParent, pBuffer->uri, dependencyPath);
        if (!pBuffer->data || !addSceneAssetCookDependency(pFingerprint, dependencyPath, pBuffer->data, pBuffer->size))
            return false;
    }
    for (cgltf_size i = 0; i < pData->images_count; ++i)
    {
        const cgltf_image* pImage = &pData->images[i];
        if (!pImage->uri || strncmp(pImage->uri, "data:", 5) == 0 || strstr(pImage->uri, "://"))
            continue;
        char dependencyPath[FS_MAX_PATH] = {};
        fsAppendPathComponent(sourceParent, pImage->uri, dependencyPath);
        if (!addSceneAssetCookFileDependency(resourceDirectory, pFingerprint, dependencyPath))
            return false;
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
    fsGetParentPath(filename, directory);
    fsCreateDirectory(resourceDir, directory, true);
}

void DirectorySearch(ResourceDirectory resourceDir, const char* subDir, const char* ext, OnFind onFindCallback, void* pUserData,
                     bool recursive)
{
    // LOGF(eDEBUG, "Exploring subdir: %s", subDir ? subDir : "Root");

    // Get all requested extension files
    char** filesInDirectory = NULL;
    int    filesFoundInDirectory = 0;
    fsGetFilesWithExtension(resourceDir, subDir ? subDir : "", ext, &filesInDirectory, &filesFoundInDirectory);
    for (int j = 0; j < filesFoundInDirectory; ++j)
    {
        if (filesInDirectory[j])
        {
            const char* file = filesInDirectory[j]; //-V595
            // LOGF(eDEBUG, "Found file: %s", file);

            onFindCallback(resourceDir, file, pUserData);
        }
    }

    if (filesInDirectory != NULL) //-V595
    {
        tf_free(filesInDirectory);
    }

    if (recursive)
    {
        char** subDirectories = NULL;
        int    subDirectoryCount = 0;
        fsGetSubDirectories(resourceDir, subDir ? subDir : "", &subDirectories, &subDirectoryCount);
        for (int i = 0; i < subDirectoryCount; ++i)
        {
            if (subDirectories[i])
            {
                const char* sDir = subDirectories[i]; //-V595
                DirectorySearch(resourceDir, sDir, ext, onFindCallback, pUserData, recursive);
            }
        }

        if (subDirectories != NULL) //-V595
        {
            tf_free(subDirectories);
        }
    }
}

#if defined(HORIZON_ENABLE_ANIMATION_COOKER)
void ReleaseSkeletonAndAnimationParams(SkeletonAndAnimations* pArray, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        bdestroy(&pArray[i].mSkeletonInFile);
        bdestroy(&pArray[i].mSkeletonOutFile);

        for (uint32_t a = 0, end = (uint32_t)arrlen(pArray[i].mAnimations); a < end; ++a)
        {
            bdestroy(&pArray[i].mAnimations[a].mInputAnim);
            bdestroy(&pArray[i].mAnimations[a].mOutputAnimPath);
        }

        arrfree(pArray[i].mAnimations);
    }
}

void ReleaseSkeletonAndAnimationParams(SkeletonAndAnimations* pStbdsArray)
{
    ReleaseSkeletonAndAnimationParams(pStbdsArray, (uint32_t)arrlen(pStbdsArray));
    arrfree(pStbdsArray);
}

cgltf_result cgltf_parse_and_load(ResourceDirectory resourceDir, const char* skeletonAsset, cgltf_data** ppData, void** ppFileData)
{
    // Import the glTF with the animation
    FileStream file = {};
    if (!fsOpenStreamFromPath(resourceDir, skeletonAsset, FM_READ, &file))
    {
        LOGF(eERROR, "Failed to open gltf file %s", skeletonAsset);
        ASSERT(false);
        return cgltf_result_file_not_found;
    }

    ssize_t fileSize = fsGetStreamFileSize(&file);
    void*   fileData = tf_malloc(fileSize);

    fsReadFromStream(&file, fileData, fileSize);

    cgltf_data*   data = NULL;
    cgltf_options options = {};
    options.memory_alloc = [](void* user, cgltf_size size)
    {
        UNREF_PARAM(user);
        return tf_malloc(size);
    };
    options.memory_free = [](void* user, void* ptr)
    {
        UNREF_PARAM(user);
        tf_free(ptr);
    };
    options.rd = resourceDir;
    cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
    fsCloseStream(&file);

    if (!VERIFYMSG(cgltf_result_success == result, "Failed to parse gltf file %s with error %u", skeletonAsset, (uint32_t)result))
    {
        tf_free(fileData);
        return result;
    }

    result = cgltf_validate(data);
    if (cgltf_result_success != result)
    {
        LOGF(eWARNING, "GLTF validation finished with error %u for file %s", (uint32_t)result, skeletonAsset);
    }

    // Load buffers located in separate files (.bin) using our file system
    for (uint32_t i = 0; i < data->buffers_count; ++i)
    {
        const char* uri = data->buffers[i].uri;

        if (!uri || data->buffers[i].data)
        {
            continue;
        }

        if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
        {
            char parent[FS_MAX_PATH] = { 0 };
            fsGetParentPath(skeletonAsset, parent);
            char path[FS_MAX_PATH] = { 0 };
            fsAppendPathComponent(parent, uri, path);
            FileStream fs = {};
            if (fsOpenStreamFromPath(resourceDir, path, FM_READ, &fs))
            {
                ASSERT(fsGetStreamFileSize(&fs) >= (ssize_t)data->buffers[i].size);
                data->buffers[i].data = tf_malloc(data->buffers[i].size);
                fsReadFromStream(&fs, data->buffers[i].data, data->buffers[i].size);
                fsCloseStream(&fs);
            }
        }
    }

    result = cgltf_load_buffers(&options, data, skeletonAsset);
    if (!VERIFYMSG(cgltf_result_success == result, "Failed to load buffers from gltf file %s with error %u", skeletonAsset,
                   (uint32_t)result))
    {
        tf_free(fileData);
        return result;
    }

    *ppData = data;
    *ppFileData = fileData;

    return result;
}

cgltf_result cgltf_write(ResourceDirectory resourceDir, const char* skeletonAsset, cgltf_data* data)
{
    cgltf_options options = {};
    options.memory_alloc = [](void* user, cgltf_size size)
    {
        UNREF_PARAM(user);
        return tf_malloc(size);
    };
    options.memory_free = [](void* user, void* ptr)
    {
        UNREF_PARAM(user);
        tf_free(ptr);
    };
    options.rd = resourceDir;
    cgltf_size expected = cgltf_write(&options, NULL, 0, data);
    char*      writeBuffer = (char*)tf_malloc(expected);
    cgltf_size actual = cgltf_write(&options, writeBuffer, expected, data);
    if (expected != actual)
    {
        LOGF(eERROR, "Error: expected %zu bytes but wrote %zu bytes.\n", expected, actual);
        return cgltf_result_invalid_gltf;
    }

    FileStream file = {};
    if (!fsOpenStreamFromPath(resourceDir, skeletonAsset, FM_WRITE, &file))
    {
        tf_free(writeBuffer);
        return cgltf_result_io_error;
    }

    fsWriteToStream(&file, writeBuffer, actual - 1);
    fsCloseStream(&file);
    tf_free(writeBuffer);

    return cgltf_result_success;
}

static void DiscoverExtraAnimationsForMesh(ResourceDirectory resourceDirInput, const char* searchRootDir,
                                           const char* extraAnimationsSubdirName, SkeletonAndAnimations* pSkeletonAndAnimations)
{
    // Find sub directory called animations
    char** assetSubDirectories = NULL;
    int    assetSubDirectoryCount = 0;
    fsGetSubDirectories(resourceDirInput, searchRootDir, &assetSubDirectories, &assetSubDirectoryCount);
    if (assetSubDirectories)
    {
        for (int k = 0; k < assetSubDirectoryCount; ++k)
        {
            const char* assetSubDir = assetSubDirectories[k];

            char subDir[FS_MAX_PATH] = {};
            fsGetPathFileName(assetSubDir, subDir);
            if (!stricmp(subDir, extraAnimationsSubdirName))
            {
                // Add all files in animations to the animation asset
                char** animationFiles = NULL;
                int    animationFileCount = 0;
                fsGetFilesWithExtension(resourceDirInput, assetSubDir, ".gltf", &animationFiles, &animationFileCount);
                if (animationFiles)
                {
                    for (int curAnim = 0; curAnim < animationFileCount; ++curAnim)
                    {
                        // Create animation output file name
                        bstring animOutputFileString = bempty();
                        bcatcstr(&animOutputFileString, "/");
                        bcatcstr(&animOutputFileString, extraAnimationsSubdirName);
                        bcatcstr(&animOutputFileString, "/");

                        SkeletonAndAnimations::AnimationFile anim = {};
                        anim.mInputAnim = bdynfromcstr(animationFiles[curAnim]);
                        anim.mOutputAnimPath = animOutputFileString;
                        arrpush(pSkeletonAndAnimations->mAnimations, anim);

                        LOGF(eINFO, "Found extra animation file '%s'", (const char*)anim.mInputAnim.data);
                    }
                    tf_free(animationFiles);
                }
                break;
            }
        }

        tf_free(assetSubDirectories);
    }
}

bool ProcessAnimations(AssetPipelineParams* assetParams, ProcessAnimationsParams* pProcessAnimationsParams)
{
    LOGF(eINFO, "Processing animations and writing to directory: %s", fsGetResourceDirectory(assetParams->mRDOutput));

    const SkeletonAndAnimations* pSkeletonAndAnims = pProcessAnimationsParams->pSkeletonAndAnims;
    for (uint32_t i = 0; i < (uint32_t)arrlen(pSkeletonAndAnims); ++i)
    {
        if (arrlen(pSkeletonAndAnims[i].mAnimations) == 0)
            LOGF(LogLevel::eWARNING, "Skeleton '%s' has no associated animations.", (char*)pSkeletonAndAnims[i].mSkeletonInFile.data);
    }

    // No assets found. Return.
    if (arrlen(pSkeletonAndAnims) == 0)
    {
        LOGF(eINFO, "No animation files found to process");
        return true;
    }

    LOGF(eINFO, "Processing %u skeleton assets and their animations...", (uint32_t)arrlen(pSkeletonAndAnims));

    // Process the found assets
    uint32_t assetsProcessed = 0;
    uint32_t assetsChecked = 0;
    bool     success = true;
    for (uint32_t i = 0, end = (uint32_t)arrlen(pSkeletonAndAnims); i < end; ++i)
    {
        const SkeletonAndAnimations* skeletonAndAnims = &pSkeletonAndAnims[i];
        const char*                  skeletonInputFile = (char*)skeletonAndAnims->mSkeletonInFile.data;

        // Create skeleton output file name
        const char* skeletonOutput = (char*)skeletonAndAnims->mSkeletonOutFile.data;

        assetsChecked++;

        // Check if the skeleton is already up-to-date
        bool processSkeleton = true;
        if (!assetParams->mSettings.force)
        {
            time_t lastModified = fsGetLastModifiedTime(assetParams->mRDInput, skeletonInputFile);
            if (assetParams->mAdditionalModifiedTime != 0)
                lastModified = max(lastModified, assetParams->mAdditionalModifiedTime);
            time_t lastProcessed = fsGetLastModifiedTime(assetParams->mRDOutput, skeletonOutput);

            if (lastModified < lastProcessed && lastProcessed != ~0u && lastProcessed > assetParams->mSettings.minLastModifiedTime)
                processSkeleton = false;
        }

        ozz::animation::Skeleton skeleton;
        if (processSkeleton)
        {
            LOGF(eINFO, "Regenerating Skeleton: %s -> %s", skeletonInputFile, skeletonOutput);

            // Process the skeleton
            if (CreateRuntimeSkeleton(assetParams->mRDInput, skeletonInputFile, assetParams->mRDOutput,
                                      pProcessAnimationsParams->mAnimationSettings.mSkeletonAndAnimOutRd, skeletonOutput, &skeleton,
                                      &assetParams->mSettings))
            {
                LOGF(eERROR, "Couldn't create Skeleton for mesh '%s' in %s. Skipping asset and all it's animations", skeletonInputFile,
                     skeletonOutput);
                success = false;
                continue;
            }

            ++assetsProcessed;
        }
        else
        {
            LOGF(eINFO, "Skeleton for mesh '%s' up to date, loading from disk: %s", skeletonInputFile, skeletonOutput);

            // Load skeleton from disk
            FileStream file = {};
            if (!fsOpenStreamFromPath(assetParams->mRDOutput, skeletonOutput, FM_READ, &file))
            {
                LOGF(eERROR, "Couldn't load Skeleton for mesh '%s' in %s. Skipping asset and all it's animations", skeletonInputFile,
                     skeletonOutput);
                success = false;
                continue; // TODO: We could add a parameter so that the user can select whether to stop processing more assets or not when
                          // encountering an error
            }
            ozz::io::IArchive archive(&file);
            archive >> skeleton;
            fsCloseStream(&file);
        }

        // Process animations
        for (uint32_t a = 0, animEnd = (uint32_t)arrlen(skeletonAndAnims->mAnimations); a < animEnd; ++a)
        {
            const SkeletonAndAnimations::AnimationFile* anim = &skeletonAndAnims->mAnimations[a];
            const char*                                 animInputFile = (char*)anim->mInputAnim.data;
            const char*                                 animOutputPath = (char*)anim->mOutputAnimPath.data;

            assetsChecked++;

            // Check if the animation is already up-to-date
            bool processAnimation = true;
            if (!assetParams->mSettings.force && !processSkeleton)
            {
                time_t lastModified = fsGetLastModifiedTime(assetParams->mRDInput, animInputFile);
                if (assetParams->mAdditionalModifiedTime != 0)
                    lastModified = max(lastModified, assetParams->mAdditionalModifiedTime);
                time_t lastProcessed = fsGetLastModifiedTime(assetParams->mRDOutput, animOutputPath);

                if (lastModified < lastProcessed && lastProcessed != ~0u && lastModified > assetParams->mSettings.minLastModifiedTime)
                    processAnimation = false;
            }

            if (processAnimation)
            {
                LOGF(eINFO, "Processing animations for mesh '%s': %s -> %s", skeletonInputFile, animInputFile, animOutputPath);

                // Process the animation
                if (CreateRuntimeAnimations(assetParams->mRDInput, animInputFile, animOutputPath, &skeleton,
                                            &pProcessAnimationsParams->mAnimationSettings, &assetParams->mSettings))
                {
                    LOGF(eERROR, "Failed to process animation for mesh '%s': %s -> %s", skeletonInputFile, animInputFile, animOutputPath);
                    success = false;
                    continue;
                }

                ++assetsProcessed;
            }
        }

        skeleton.Deallocate();
    }

    LOGF(LogLevel::eINFO, "ProcessAnimations: checked %u assets, regenerated %u assets.", assetsChecked, assetsProcessed);
    return !success;
}
#endif

#if defined(HORIZON_ENABLE_TRESSFX_COOKER)
bool ProcessTFX(AssetPipelineParams* assetParams, ProcessTressFXParams* tfxParams)
{
    cgltf_result result = cgltf_result_success;

    // Get all tfx files
    char** tfxFiles = NULL;
    int    tfxFileCount = 0;
    if (assetParams->mPathMode == PROCESS_MODE_DIRECTORY)
    {
        fsGetFilesWithExtension(assetParams->mRDInput, "", "tfx", &tfxFiles, &tfxFileCount);
    }
    else
    {
        tfxFiles = (char**)tf_malloc(sizeof(char**));
        tfxFiles[0] = (char*)assetParams->mInFilePath;
        tfxFileCount = 1;
    }

#define RETURN_IF_TFX_ERROR(expression)     \
    if (!(expression))                      \
    {                                       \
        LOGF(eERROR, "Failed to load tfx"); \
        return false;                       \
    }

    for (int i = 0; i < tfxFileCount; ++i)
    {
        const char* input = tfxFiles[i];
        char        outputTemp[FS_MAX_PATH] = {};
        fsGetPathFileName(input, outputTemp);
        char output[FS_MAX_PATH] = {};
        fsAppendPathExtension(outputTemp, "gltf", output);

        char binFilePath[FS_MAX_PATH] = {};
        fsAppendPathExtension(outputTemp, "bin", binFilePath);

        FileStream tfxFile = {};
        fsOpenStreamFromPath(assetParams->mRDInput, input, FM_READ, &tfxFile);
        AMD::TressFXAsset tressFXAsset = {};
        RETURN_IF_TFX_ERROR(tressFXAsset.LoadHairData(&tfxFile))
        fsCloseStream(&tfxFile);

        if (tfxParams->mFollowHairCount)
        {
            RETURN_IF_TFX_ERROR(tressFXAsset.GenerateFollowHairs(tfxParams->mFollowHairCount, tfxParams->mTipSeperationFactor,
                                                                 tfxParams->mMaxRadiusAroundGuideHair))
        }

        RETURN_IF_TFX_ERROR(tressFXAsset.ProcessAsset())

        struct TypePair
        {
            cgltf_type           type;
            cgltf_component_type comp;
        };
        const TypePair vertexTypes[] = {
            { cgltf_type_scalar, cgltf_component_type_r_32u }, // Indices
            { cgltf_type_vec4, cgltf_component_type_r_32f },   // Position
            { cgltf_type_vec4, cgltf_component_type_r_32f },   // Tangents
            { cgltf_type_vec4, cgltf_component_type_r_32f },   // Global rotations
            { cgltf_type_vec4, cgltf_component_type_r_32f },   // Local rotations
            { cgltf_type_vec4, cgltf_component_type_r_32f },   // Ref vectors
            { cgltf_type_vec4, cgltf_component_type_r_32f },   // Follow root offsets
            { cgltf_type_vec2, cgltf_component_type_r_32f },   // Strand UVs
            { cgltf_type_scalar, cgltf_component_type_r_32u }, // Strand types
            { cgltf_type_scalar, cgltf_component_type_r_32f }, // Thickness coeffs
            { cgltf_type_scalar, cgltf_component_type_r_32f }, // Rest lengths
        };
        const uint32_t vertexStrides[] = {
            sizeof(uint32_t), // Indices
            sizeof(float4),   // Position
            sizeof(float4),   // Tangents
            sizeof(float4),   // Global rotations
            sizeof(float4),   // Local rotations
            sizeof(float4),   // Ref vectors
            sizeof(float4),   // Follow root offsets
            sizeof(float2),   // Strand UVs
            sizeof(uint32_t), // Strand types
            sizeof(float),    // Thickness coeffs
            sizeof(float),    // Rest lengths
        };
        const uint32_t vertexCounts[] = {
            (uint32_t)tressFXAsset.GetNumHairTriangleIndices(), // Indices
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Position
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Tangents
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Global rotations
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Local rotations
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Ref vectors
            (uint32_t)tressFXAsset.m_numTotalStrands,           // Follow root offsets
            (uint32_t)tressFXAsset.m_numTotalStrands,           // Strand UVs
            (uint32_t)tressFXAsset.m_numTotalStrands,           // Strand types
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Thickness coeffs
            (uint32_t)tressFXAsset.m_numTotalVertices,          // Rest lengths
        };
        const void* vertexData[] = {
            tressFXAsset.m_triangleIndices,   // Indices
            tressFXAsset.m_positions,         // Position
            tressFXAsset.m_tangents,          // Tangents
            tressFXAsset.m_globalRotations,   // Global rotations
            tressFXAsset.m_localRotations,    // Local rotations
            tressFXAsset.m_refVectors,        // Ref vectors
            tressFXAsset.m_followRootOffsets, // Follow root offsets
            tressFXAsset.m_strandUV,          // Strand UVs
            tressFXAsset.m_strandTypes,       // Strand types
            tressFXAsset.m_thicknessCoeffs,   // Thickness coeffs
            tressFXAsset.m_restLengths,       // Rest lengths
        };
        const char* vertexNames[] = {
            "INDEX",      // Indices
            "POSITION",   // Position
            "TANGENT",    // Tangents
            "TEXCOORD_0", // Global rotations
            "TEXCOORD_1", // Local rotations
            "TEXCOORD_2", // Ref vectors
            "TEXCOORD_3", // Follow root offsets
            "TEXCOORD_4", // Strand UVs
            "TEXCOORD_5", // Strand types
            "TEXCOORD_6", // Thickness coeffs
            "TEXCOORD_7", // Rest lengths
        };
        const uint32_t count = sizeof(vertexData) / sizeof(vertexData[0]);

        cgltf_buffer      buffer = {};
        cgltf_accessor    accessors[count] = {};
        cgltf_buffer_view views[count] = {};
        cgltf_attribute   attribs[count] = {};
        cgltf_mesh        mesh = {};
        cgltf_primitive   prim = {};
        cgltf_size        offset = 0;
        FileStream        binFile = {};
        fsOpenStreamFromPath(assetParams->mRDOutput, binFilePath, FM_WRITE, &binFile);
        size_t fileSize = 0;

        for (uint32_t j = 0; j < count; ++j)
        {
            views[j].type = (j ? cgltf_buffer_view_type_vertices : cgltf_buffer_view_type_indices);
            views[j].buffer = &buffer;
            views[j].offset = offset;
            views[j].size = vertexCounts[j] * vertexStrides[j];
            accessors[j].component_type = vertexTypes[j].comp;
            accessors[j].stride = vertexStrides[j];
            accessors[j].count = vertexCounts[j];
            accessors[j].offset = 0;
            accessors[j].type = vertexTypes[j].type;
            accessors[j].buffer_view = &views[j];

            attribs[j].name = (char*)vertexNames[j];
            attribs[j].data = &accessors[j];

            fileSize += fsWriteToStream(&binFile, vertexData[j], views[j].size);
            offset += views[j].size;
        }
        fsCloseStream(&binFile);

        char uri[FS_MAX_PATH] = {};
        fsGetPathFileName(binFilePath, uri);
        // sprintf(uri, "%s", fn.buffer);
        buffer.uri = uri;
        buffer.size = fileSize;

        prim.indices = accessors;
        prim.attributes_count = count - 1;
        prim.attributes = attribs + 1;
        prim.type = cgltf_primitive_type_triangles;

        mesh.primitives_count = 1;
        mesh.primitives = &prim;

        char extras[128] = {};
        snprintf(extras, sizeof extras, "{ \"%s\" : %d, \"%s\" : %d }", "mVertexCountPerStrand", tressFXAsset.m_numVerticesPerStrand,
                 "mGuideCountPerStrand", tressFXAsset.m_numGuideStrands);

        char       generator[] = "TressFX";
        cgltf_data data = {};
        data.asset.generator = generator;
        data.buffers_count = 1;
        data.buffers = &buffer;
        data.buffer_views_count = count;
        data.buffer_views = views;
        data.accessors_count = count;
        data.accessors = accessors;
        data.meshes_count = 1;
        data.meshes = &mesh;
        data.file_data = extras;
        data.asset.extras.start_offset = 0;
        data.asset.extras.end_offset = strlen(extras);
        result = cgltf_write(assetParams->mRDOutput, output, &data);
    }

    if (tfxFiles)
    {
        tf_free(tfxFiles);
    }

    return result != cgltf_result_success;
}
#else
bool ProcessTFX(AssetPipelineParams*, ProcessTressFXParams*)
{
    LOGF(eERROR, "TressFX cooking is unavailable because TressFX is not vendored in this repository.");
    return true;
}
#endif

#if defined(HORIZON_ENABLE_ANIMATION_COOKER)
static uint32_t FindJoint(ozz::animation::Skeleton* skeleton, const char* name)
{
    for (int i = 0; i < skeleton->num_joints(); i++)
    {
        if (strcmp(skeleton->joint_names()[i], name) == 0)
            return i;
    }
    return UINT_MAX;
}

static bool isTrsDecomposable(const Matrix4& matrix)
{
    if (matrix.getCol0().getW() != 0.0 || matrix.getCol1().getW() != 0.0 || matrix.getCol2().getW() != 0.0 ||
        matrix.getCol3().getW() != 1.0)
    {
        return false;
    }

    if (determinant(matrix) == 0.0)
    {
        return false;
    }

    return true;
}

bool CreateRuntimeSkeleton(ResourceDirectory resourceDirInput, const char* skeletonInputfile, ResourceDirectory resourceDirGltfOutput,
                           ResourceDirectory resourceDirOzzOutput, const char* skeletonOutputFile, ozz::animation::Skeleton* pOutSkeleton,
                           ProcessAssetsSettings* settings)
{
    UNREF_PARAM(settings);
    cgltf_data*  data = NULL;
    void*        srcFileData = NULL;
    cgltf_result result = cgltf_parse_and_load(resourceDirInput, skeletonInputfile, &data, &srcFileData);
    if (cgltf_result_success != result)
    {
        LOGF(LogLevel::eERROR, "gltf parsing failed: %s", skeletonInputfile);
        return true;
    }

    if (data->skins_count == 0)
    {
        data->file_data = srcFileData;
        cgltf_free(data);
        LOGF(LogLevel::eERROR, "Rigged mesh %s has no bones. Skeleton can not be created.", skeletonInputfile);
        return true;
    }

    // Gather node info
    // Used to mark nodes that should be included in the skeleton
    SkeletonNodeInfo*      nodeData = nullptr;
    size_t                 startIndex = data->nodes_count - 1;
    const SkeletonNodeInfo startNode{ bdynfromcstr(data->nodes[startIndex].name), &data->nodes[startIndex] };
    arrpush(nodeData, startNode);

    const uint32_t queueSize = (uint32_t)(data->nodes_count + 1);
    int32_t*       nodeQueue = nullptr; // Simple queue because tinystl doesn't have one
    arrsetlen(nodeQueue, queueSize);
    for (uint32_t i = 0; i < queueSize; ++i)
        nodeQueue[i] = -1;
    nodeQueue[0] = 0;
    uint32_t nodeQueueStart = 0;
    uint32_t nodeQueueEnd = 1;
    while (nodeQueue[nodeQueueStart] != -1)
    {
        // Pop
        int32_t     nodeIndex = nodeQueue[nodeQueueStart];
        cgltf_node* node = nodeData[nodeIndex].pNode;
        nodeQueue[nodeQueueStart] = -1;

        for (uint i = 0; i < node->children_count; ++i)
        {
            SkeletonNodeInfo childNode = {};
            childNode.mName = bdynfromcstr(node->children[i]->name);
            childNode.pParentNodeIndex = nodeIndex;
            const int32_t childNodeIndex = (int32_t)arrlen(nodeData);
            childNode.pNode = node->children[i];
            arrpush(nodeData, childNode);

            arrpush(nodeData[nodeIndex].mChildNodeIndices, childNodeIndex);

            const uint32_t queueIdx = nodeQueueEnd;
            ASSERT(queueIdx != nodeQueueStart && "Seems like we didn't allocate enough space for the queue");
            if (queueIdx == nodeQueueStart)
            {
                LOGF(LogLevel::eERROR, "Too many nodes in scene. Skeleton '%s' can not be created.", skeletonInputfile);
                return true;
            }

            nodeQueueEnd = (nodeQueueEnd + 1) % queueSize;

            // Push
            nodeQueue[queueIdx] = childNodeIndex;
        }

        nodeQueueStart++;
        ASSERT(nodeQueueStart < (uint32_t)arrlen(nodeQueue) && "Seems like we didn't allocate enough space for the queue");
    }

    arrfree(nodeQueue);
    nodeQueue = nullptr;

    // Mark all nodes that are required to be in the skeleton
    for (cgltf_size i = 0; i < data->skins_count; ++i)
    {
        cgltf_skin* skin = &data->skins[i];
        for (cgltf_size j = 0; j < skin->joints_count; ++j)
        {
            cgltf_node* bone = skin->joints[j];
            for (cgltf_size k = 0; k < (cgltf_size)arrlen(nodeData); ++k)
            {
                if (strcmp((const char*)nodeData[k].mName.data, bone->name) == 0)
                {
                    int32_t nodeIndex = (int32_t)k;
                    while (nodeIndex != -1)
                    {
                        if (nodeData[nodeIndex].mUsedInSkeleton)
                            break; // Remaining part of the tree is already marked
                        nodeData[nodeIndex].mUsedInSkeleton = true;
                        nodeIndex = nodeData[nodeIndex].pParentNodeIndex;
                    }
                }
            }
        }
    }

    // Create raw skeleton
    ozz::animation::offline::RawSkeleton rawSkeleton = {};

    SkeletonBoneInfo*      boneData = nullptr;
    const SkeletonBoneInfo firstBone{ 0, -1, NULL };
    arrpush(boneData, firstBone);

    while (arrlen(boneData) != 0)
    {
        SkeletonBoneInfo boneInfo = arrlast(boneData);
        arrpop(boneData);
        SkeletonNodeInfo* nodeInfo = &nodeData[boneInfo.mNodeIndex];
        cgltf_node*       node = nodeInfo->pNode;

        // Get node transform

        // Create joint from node
        ozz::animation::offline::RawSkeleton::Joint joint;

        if (!node->has_matrix)
        {
            joint.transform.translation = Vector3(node->translation[0], node->translation[1], node->translation[2]);
            joint.transform.rotation = Quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
            joint.transform.scale =
                Vector3(node->scale[0], node->scale[1], node->scale[2]); // *(boneInfo.mParentNodeIndex == -1 ? 0.01f : 1.f);
        }
        else
        {
            // Matrix Decomposition
            Matrix4 mat;
            mat.setCol0(Vector4(node->matrix[0], node->matrix[1], node->matrix[2], node->matrix[3]));
            mat.setCol1(Vector4(node->matrix[4], node->matrix[5], node->matrix[6], node->matrix[7]));
            mat.setCol2(Vector4(node->matrix[8], node->matrix[9], node->matrix[10], node->matrix[11]));
            mat.setCol3(Vector4(node->matrix[12], node->matrix[13], node->matrix[14], node->matrix[15]));

            if (isTrsDecomposable(mat))
            {
                // extract translation
                joint.transform.translation = mat.getTranslation();

                // extract the scaling factors from columns of the matrix
                Matrix3 upperMat = mat.getUpper3x3();
                Vector3 pScaling = Vector3(length(upperMat.getCol0()), length(upperMat.getCol1()), length(upperMat.getCol2()));

                // and the sign of the scaling
                if (determinant(mat) < 0)
                    pScaling = -pScaling;
                joint.transform.scale = pScaling; //*(boneInfo.mParentNodeIndex == -1 ? 0.01f : 1.f);

                // and remove all scaling from the matrix
                if (pScaling.getX())
                    upperMat.setCol0(upperMat.getCol0() / pScaling.getX());
                if (pScaling.getY())
                    upperMat.setCol1(upperMat.getCol1() / pScaling.getY());
                if (pScaling.getZ())
                    upperMat.setCol2(upperMat.getCol2() / pScaling.getZ());

                // and generate the rotation quaternion from it
                joint.transform.rotation = Quat(upperMat);
            }
        }

        bassign(&joint.name, &nodeInfo->mName);

        // Add node to raw skeleton
        ozz::animation::offline::RawSkeleton::Joint* newParentJoint = NULL;
        if (boneInfo.pParentJoint == NULL)
        {
            arrpush(rawSkeleton.roots, joint);
            newParentJoint = &arrlast(rawSkeleton.roots);
        }
        else
        {
            arrpush(boneInfo.pParentJoint->children, joint);
            newParentJoint = &arrlast(boneInfo.pParentJoint->children);
        }

        // Count the child nodes that are required to be in the skeleton
        int requiredChildCount = 0;
        for (uint i = 0; i < (uint)arrlen(nodeInfo->mChildNodeIndices); ++i)
        {
            SkeletonNodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
            if (childNodeInfo->mUsedInSkeleton)
                ++requiredChildCount;
        }

        // Add child nodes to the list of nodes to process
        arrsetcap(newParentJoint->children, requiredChildCount); // Reserve to make sure memory isn't reallocated later.
        for (uint i = 0; i < (uint)arrlen(nodeInfo->mChildNodeIndices); ++i)
        {
            SkeletonNodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
            if (childNodeInfo->mUsedInSkeleton)
            {
                boneInfo.mNodeIndex = nodeInfo->mChildNodeIndices[i];
                boneInfo.mParentNodeIndex = boneInfo.mNodeIndex;
                boneInfo.pParentJoint = newParentJoint;
                arrpush(boneData, boneInfo);
            }
        }
    }

    for (uint32_t i = 0, end = (uint32_t)arrlen(nodeData); i < end; ++i)
    {
        bdestroy(&nodeData[i].mName);
        arrfree(nodeData[i].mChildNodeIndices);
    }

    arrfree(boneData);
    boneData = nullptr;
    arrfree(nodeData);
    nodeData = nullptr;

    // Validate raw skeleton
    if (!rawSkeleton.Validate())
    {
        LOGF(LogLevel::eERROR, "Skeleton created for %s is invalid. Skeleton can not be created.", skeletonInputfile);
        return true;
    }

    // Build runtime skeleton from raw skeleton
    if (!ozz::animation::offline::SkeletonBuilder::Build(rawSkeleton, pOutSkeleton))
    {
        LOGF(LogLevel::eERROR, "Failed to build Skeleton: %s", skeletonInputfile);
        return true;
    }

    // Ensure output dir exists before we write the skeleton to disk
    {
        char directory[FS_MAX_PATH] = {};
        fsGetParentPath(skeletonOutputFile, directory);
        fsCreateDirectory(resourceDirOzzOutput, directory, true);
    }

    // Write skeleton to disk
    FileStream file = {};
    if (!fsOpenStreamFromPath(resourceDirOzzOutput, skeletonOutputFile, FM_WRITE, &file))
        return true;

    ozz::io::OArchive archive(&file);
    archive << *pOutSkeleton;
    fsCloseStream(&file);

    // Generate joint remaps in the gltf
    {
        char*    buffer = nullptr;
        uint32_t size = 0;

        for (uint32_t i = 0; i < data->meshes_count; ++i)
        {
            cgltf_mesh* mesh = &data->meshes[i];
            cgltf_node* node = NULL;
            for (uint32_t n = 0; n < data->nodes_count; ++n)
            {
                if (data->nodes[n].mesh == mesh)
                {
                    node = &data->nodes[n];
                    break;
                }
            }

            if (node && node->mesh && node->skin)
            {
                cgltf_skin* skin = node->skin;

                char     jointRemaps[1024] = {};
                uint32_t offset = 0;
                offset += snprintf(jointRemaps + offset, sizeof(jointRemaps) - offset, "[ ");

                for (uint32_t j = 0; j < skin->joints_count; ++j)
                {
                    const cgltf_node* jointNode = skin->joints[j];
                    uint32_t          jointIndex = FindJoint(pOutSkeleton, jointNode->name);
                    if (j == 0)
                        offset += snprintf(jointRemaps + offset, sizeof(jointRemaps) - offset, "%u", jointIndex);
                    else
                        offset += snprintf(jointRemaps + offset, sizeof(jointRemaps) - offset, ", %u", jointIndex);
                }

                offset += snprintf(jointRemaps + offset, sizeof(jointRemaps) - offset, " ]");
                skin->extras.start_offset = size;
                size += (uint32_t)strlen(jointRemaps) + 1;
                skin->extras.end_offset = size - 1;

                const size_t iterCount = strlen(jointRemaps) + 1;
                for (uint32_t j = 0; j < iterCount; ++j)
                    arrpush(buffer, jointRemaps[j]);
            }
        }

        for (uint32_t i = 0; i < data->buffers_count; ++i)
        {
            if (strncmp(data->buffers[i].uri, "://", 3) == 0)
            {
                char parentPath[FS_MAX_PATH] = {};
                fsGetParentPath(skeletonInputfile, parentPath);
                char bufferOut[FS_MAX_PATH] = {};
                fsAppendPathComponent(parentPath, data->buffers[i].uri, bufferOut);

                LOGF(eINFO, "Writing binary gltf data: %s -> %s", skeletonInputfile, bufferOut);

                FileStream fs = {};
                fsOpenStreamFromPath(resourceDirGltfOutput, bufferOut, FM_WRITE, &fs);
                fsWriteToStream(&fs, data->buffers[i].data, data->buffers[i].size);
                fsCloseStream(&fs);
            }
        }

        LOGF(eINFO, "Writing text gltf data: %s -> %s", skeletonInputfile, skeletonInputfile);

        data->file_data = buffer;
        if (cgltf_result_success != cgltf_write(resourceDirGltfOutput, skeletonInputfile, data))
        {
            LOGF(eERROR, "Couldn't write the gltf file with the remapped joint data for it's skeleton.");
            arrfree(buffer);
            return true;
        }

        arrfree(buffer);
    }

    data->file_data = srcFileData;
    cgltf_free(data);

    const bool error = false;
    return error;
}

static bool CreateRuntimeAnimation(RuntimeAnimationSettings* animationSettings, cgltf_animation* animationData, const char* animOutFile,
                                   ozz::animation::Skeleton* skeleton, const char* animationSourceFile)
{
    ASSERT(animationData && skeleton);

    // Create raw animation
    ozz::animation::offline::RawAnimation rawAnimation = {};
    rawAnimation.name = bdynfromcstr(animationData->name);
    rawAnimation.duration = 0.f;

    arrsetlen(rawAnimation.tracks, skeleton->num_joints());
    memset(rawAnimation.tracks, 0, sizeof(*rawAnimation.tracks) * skeleton->num_joints());

    for (uint i = 0; i < (uint)skeleton->num_joints(); ++i)
    {
        const char* jointName = skeleton->joint_names()[i];

        ozz::animation::offline::RawAnimation::JointTrack* track = &rawAnimation.tracks[i];

        for (cgltf_size channelIndex = 0; channelIndex < animationData->channels_count; channelIndex += 1)
        {
            cgltf_animation_channel* channel = &animationData->channels[channelIndex];

            if (strcmp(channel->target_node->name, jointName) != 0)
                continue;

            if (channel->target_path == cgltf_animation_path_type_translation)
            {
                arrsetlen(track->translations, channel->sampler->output->count);
                memset((char*)track->translations, 0, sizeof(*track->translations) * channel->sampler->output->count);
                for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
                {
                    float time = 0.0;
                    Vector3 translation = Vector3(0.0f);
                    cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
                    cgltf_accessor_read_float(channel->sampler->output, j, (float*)&translation, 3);

                    track->translations[j] = { time, translation };
                }
            }

            if (channel->target_path == cgltf_animation_path_type_rotation)
            {
                arrsetlen(track->rotations, channel->sampler->output->count);
                memset((char*)track->rotations, 0, sizeof(*track->rotations) * channel->sampler->output->count);
                for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
                {
                    float time = 0.0;
                    Quat  rotation = Quat(0.0f);
                    cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
                    cgltf_accessor_read_float(channel->sampler->output, j, (float*)&rotation, 4);

                    track->rotations[j] = { time, rotation };
                }
            }

            if (channel->target_path == cgltf_animation_path_type_scale)
            {
                arrsetlen(track->scales, channel->sampler->output->count);
                memset((char*)track->scales, 0, sizeof(*track->scales) * channel->sampler->output->count);
                for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
                {
                    float time = 0.0;
                    Vector3 scale = Vector3(0.0f);
                    cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
                    cgltf_accessor_read_float(channel->sampler->output, j, (float*)&scale, 3);

                    track->scales[j] = { time, scale };
                }
            }
        }

        if (arrlen(track->translations) != 0)
            rawAnimation.duration = max(rawAnimation.duration, arrlast(track->translations).time);

        if (arrlen(track->rotations) != 0)
            rawAnimation.duration = max(rawAnimation.duration, arrlast(track->rotations).time);

        if (arrlen(track->scales) != 0)
            rawAnimation.duration = max(rawAnimation.duration, arrlast(track->scales).time);
    }

    // Validate raw animation
    if (!rawAnimation.Validate())
    {
        LOGF(LogLevel::eERROR, "Animation %s:%s is invalid. Animation can not be created.", animationSourceFile, animationData->name);
        return true;
    }

    ozz::animation::offline::RawAnimation* rawAnimToBuild = &rawAnimation;

    ozz::animation::offline::RawAnimation optimizedRawAnimation = {};
    if (animationSettings->mOptimizeTracks)
    {
        LOGF(eINFO, "Optimizing tracks...");
        ozz::animation::offline::AnimationOptimizer optimizer = {};
        optimizer.setting.tolerance = animationSettings->mOptimizationTolerance;
        optimizer.setting.distance = animationSettings->mOptimizationDistance;
        if (!optimizer(rawAnimation, *skeleton, &optimizedRawAnimation))
        {
            LOGF(LogLevel::eERROR, "Animation %s:%s could not be optimized for skeleton.", animationSourceFile, animationData->name);
            return true;
        }

        rawAnimToBuild = &optimizedRawAnimation;
    }

    // Build runtime animation from raw animation
    ozz::animation::Animation animation = {};
    if (!ozz::animation::offline::AnimationBuilder::Build(*rawAnimToBuild, &animation))
    {
        LOGF(LogLevel::eERROR, "Animation %s:%s can not be created.", animationSourceFile, animationData->name);
        return true;
    }

    // Ensure output dir exists before we write the animations to disk
    {
        char directory[FS_MAX_PATH] = {};
        fsGetParentPath(animOutFile, directory);
        fsCreateDirectory(animationSettings->mSkeletonAndAnimOutRd, directory, true);
    }

    // Write animation to disk
    FileStream file = {};
    if (!fsOpenStreamFromPath(animationSettings->mSkeletonAndAnimOutRd, animOutFile, FM_WRITE, &file))
    {
        LOGF(LogLevel::eERROR, "Animation %s:%s can not be saved to %s/%s", animationSourceFile, animationData->name,
             fsGetResourceDirectory(animationSettings->mSkeletonAndAnimOutRd), animOutFile);
        animation.Deallocate();
        return true;
    }

    ozz::io::OArchive archive(&file);
    archive << animation;
    fsCloseStream(&file);
    // Deallocate animation
    animation.Deallocate();

    return false;
}

bool CreateRuntimeAnimations(ResourceDirectory resourceDirInput, const char* animationInputFile, const char* animationOutputPath,
                             ozz::animation::Skeleton* skeleton, RuntimeAnimationSettings* animationSettings,
                             ProcessAssetsSettings* settings)
{
    UNREF_PARAM(settings);
    // Import the glTF with the animation
    cgltf_data*  data = NULL;
    void*        srcFileData = NULL;
    cgltf_result result = cgltf_parse_and_load(resourceDirInput, animationInputFile, &data, &srcFileData);
    if (result != cgltf_result_success)
    {
        return true;
    }

    // Check if the asset contains any animations
    if (data->animations_count == 0)
    {
        LOGF(LogLevel::eWARNING, "Animation asset %s contains no animations.", animationInputFile);

        tf_free(srcFileData);
        cgltf_free(data);
        return true;
    }

    LOGF(eINFO, "Processing %u animations from file '%s'", (uint32_t)data->animations_count, animationInputFile);

    bool error = false;
    for (cgltf_size animationIndex = 0; animationIndex < data->animations_count; animationIndex += 1)
    {
        cgltf_animation* animationData = &data->animations[animationIndex];

        LOGF(eINFO, "Processing animation %u: %s", (uint32_t)animationIndex, animationData->name);

        char buffer[FS_MAX_PATH] = {};
        if (data->animations_count == 1)
        {
            // If the file only contains one animation we use the name of the input file as the output file
            char inAnimFilename[FS_MAX_PATH / 2] = {};
            fsGetPathFileName(animationInputFile, inAnimFilename);
            snprintf(buffer, sizeof(buffer), "%s/%s.ozz", animationOutputPath, inAnimFilename);
        }
        else
        {
            // When the input file contains more than one animation we use the name of the animation so that we don't have collisions
            snprintf(buffer, sizeof(buffer), "%s/%s.ozz", animationOutputPath, animationData->name);
        }

        const char* animOutFilePath = buffer;
        if (CreateRuntimeAnimation(animationSettings, animationData, animOutFilePath, skeleton, animationInputFile))
            error = true;
    }

    tf_free(srcFileData);
    cgltf_free(data);

    return error;
}
#endif

static inline constexpr ShaderSemantic util_cgltf_attrib_type_to_semantic(cgltf_attribute_type type, uint32_t index)
{
    switch (type)
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
        return SEMANTIC_TEXCOORD0;
    }
}

static inline constexpr hz::Format util_cgltf_type_to_image_format(cgltf_type type, cgltf_component_type compType)
{
    switch (type)
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

typedef void (*PackingFunction)(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst);

static inline void util_pack_float2_to_half2(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src,
                                             uint8_t* dst)
{
    COMPILE_ASSERT(sizeof(float2) == sizeof(float[2]));
    ASSERT(srcStride == sizeof(float2));
    ASSERT(dstStride == sizeof(uint32_t));

    float2* f = (float2*)src;
    for (uint32_t e = 0; e < count; ++e)
    {
        *(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = packFloat2ToHalf2(f[e]);
    }
}

static inline void util_pack_float3_direction_to_half2(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset,
                                                       const uint8_t* src, uint8_t* dst)
{
    COMPILE_ASSERT(sizeof(float3) == sizeof(float[3]));
    ASSERT(dstStride == sizeof(uint32_t));
    for (uint32_t e = 0; e < count; ++e)
    {
        float3 f = *(float3*)(src + e * srcStride);
        *(uint32_t*)(dst + e * dstStride + offset) = packFloat3DirectionToHalf2(f);
    }
}

static inline void util_unpack_uint8_to_uint16_joints(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset,
                                                      const uint8_t* src, uint8_t* dst)
{
    ASSERT(srcStride == 4 && "Expecting stride of 4 (sizeof(uint8_t) * 4 joints)");

    for (uint32_t e = 0; e < count; ++e)
    {
        const uint8_t* srcBase = (const uint8_t*)(src + e * srcStride);
        uint16_t*      dstBase = (uint16_t*)(dst + e * dstStride + offset);
        dstBase[0] = srcBase[0];
        dstBase[1] = srcBase[1];
        dstBase[2] = srcBase[2];
        dstBase[3] = srcBase[3];
    }
}

void OnGLTFFind(ResourceDirectory resourceDir, const char* filename, void* pUserData)
{
    UNREF_PARAM(resourceDir);
    bstring** inputFileNames = (bstring**)pUserData;
    arrpush(*inputFileNames, bdynfromcstr(filename));
}

void CheckIfGltfHasAnimations(ResourceDirectory resourceDirInput, const char* gltfFileName, uint32_t& numAnimationsOut)
{
    numAnimationsOut = 0;

    FileStream file = {};
    if (!fsOpenStreamFromPath(resourceDirInput, gltfFileName, FM_READ, &file))
    {
        LOGF(eERROR, "Failed to open gltf file %s", gltfFileName);
        return;
    }

    ssize_t fileSize = fsGetStreamFileSize(&file);
    void*   fileData = tf_malloc(fileSize);

    fsReadFromStream(&file, fileData, fileSize);

    cgltf_options options = {};
    cgltf_data*   data = NULL;
    options.memory_alloc = [](void* user, cgltf_size size)
    {
        UNREF_PARAM(user);
        return tf_malloc(size);
    };
    options.memory_free = [](void* user, void* ptr)
    {
        UNREF_PARAM(user);
        tf_free(ptr);
    };
    options.rd = resourceDirInput;
    cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
    fsCloseStream(&file);

    if (cgltf_result_success != result)
    {
        LOGF(eERROR, "Failed to parse gltf file %s with error %u", gltfFileName, (uint32_t)result);
        tf_free(fileData);
        return;
    }

    numAnimationsOut = (uint32_t)data->animations_count;

    cgltf_free(data);
    tf_free(fileData);
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
        LOGF(eERROR, "Failed to build meshlets");
    }
}

static void geomOptimize(GeometryData* geomData, MeshOptimizerFlags optimizationFlags, IndexType indexType, uint32_t indexOffset,
                         uint32_t indexCount, uint32_t vertexOffset, uint32_t* vertexCount)
{
    if (optimizationFlags == MESH_OPTIMIZATION_FLAG_OFF)
        return;

    size_t remapSize = (*vertexCount * sizeof(uint32_t));

    uint32_t* remap = (uint32_t*)tf_malloc(remapSize);

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

    // Remap functions expect the same vertex count passed when generating the map, so we have to set the new vertex count later on
    uint32_t newVertCount = 0;

    // generating remap & new vertex/index sets
    if (indexType == INDEX_TYPE_UINT16)
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
    if (indexType == INDEX_TYPE_UINT16)
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
    tf_free(remap);
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
            fsGetPathFileName(fileName, extractedFileName);

            strcat(extractedFileName, ".bin");
            fsAppendPathComponent(assetParams->mOutSubdir, extractedFileName, newFileName);
        }
        else
        {
            fsReplacePathExtension(fileName, "bin", newFileName);
        }

        LOGF(eINFO, "Converting %s to TF custom binary file", fileName);

        FileStream file = {};
        if (!fsOpenStreamFromPath(assetParams->mRDInput, fileName, FM_READ, &file))
        {
            LOGF(eERROR, "Failed to open gltf file %s", fileName);
            error = true;
            continue;
        }

        ssize_t fileSize = fsGetStreamFileSize(&file);
        void*   fileData = tf_malloc(fileSize);

        fsReadFromStream(&file, fileData, fileSize);

        cgltf_options options = {};
        cgltf_data*   data = NULL;
        options.memory_alloc = [](void* user, cgltf_size size)
        {
            UNREF_PARAM(user);
            return tf_malloc(size);
        };
        options.memory_free = [](void* user, void* ptr)
        {
            UNREF_PARAM(user);
            tf_free(ptr);
        };
        options.rd = assetParams->mRDInput;
        cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
        fsCloseStream(&file);

        if (cgltf_result_success != result)
        {
            LOGF(eERROR, "Failed to parse gltf file %s with error %u", fileName, (uint32_t)result);
            tf_free(fileData);
            error = true;
            continue;
        }

        result = cgltf_validate(data);
        if (cgltf_result_success != result)
        {
            LOGF(eERROR, "Invalid glTF file %s (error %u)", fileName, (uint32_t)result);
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
                fsGetParentPath(fileName, parent);
                char path[FS_MAX_PATH] = { 0 };
                fsAppendPathComponent(parent, uri, path);
                FileStream fs = {};
                if (fsOpenStreamFromPath(assetParams->mRDInput, path, FM_READ, &fs))
                {
                    buffersLoaded = fsGetStreamFileSize(&fs) >= (ssize_t)data->buffers[j].size;
                    if (buffersLoaded)
                    {
                        data->buffers[j].data = tf_malloc(data->buffers[j].size);
                        buffersLoaded = fsReadFromStream(&fs, data->buffers[j].data, data->buffers[j].size) == data->buffers[j].size;
                    }
                    fsCloseStream(&fs);
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
            LOGF(eERROR, "Failed to load buffers from gltf file %s with error %u", fileName, (uint32_t)result);
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        SceneAssetCookFingerprint* pFingerprint = (SceneAssetCookFingerprint*)tf_calloc(1, sizeof(SceneAssetCookFingerprint));
        if (!pFingerprint || !buildSceneAssetCookFingerprint(assetParams->mRDInput, fileName, fileData, (size_t)fileSize, data, glTFParams,
                                                             assetParams->mAdditionalModifiedTime, pFingerprint))
        {
            LOGF(eERROR, "Failed to fingerprint glTF dependencies for %s", fileName);
            tf_free(pFingerprint);
            data->file_data = fileData;
            cgltf_free(data);
            error = true;
            continue;
        }

        char contentHash[kSceneAssetCookHashCapacity] = {};
        formatSceneAssetCookHash(pFingerprint, contentHash);
        SceneAssetCooker sceneCooker(assetParams->mRDInput, fileName, assetParams->mRDOutput, newFileName);
        if (!assetParams->mSettings.force && fsFileExist(assetParams->mRDOutput, newFileName) && sceneCooker.isCurrent(contentHash))
        {
            LOGF(eINFO, "Skipping %s (content hash unchanged)", fileName);
            tf_free(pFingerprint);
            data->file_data = fileData;
            cgltf_free(data);
            continue;
        }

        if (!sceneCooker.prepare(*data) || !sceneCooker.build(*data, contentHash))
        {
            tf_free(pFingerprint);
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

        // Find number of traditional draw calls required to draw this piece of geometry
        // Find total index count, total vertex count
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
                    ASSERT(semanticIdx < MAX_SEMANTICS);
                    vertexAttribs[semanticIdx] = &prim->attributes[k];
                    vertexAttribCount[semanticIdx] += (uint32_t)prim->attributes[k].data->count;
                }
            }
        }

        // Request the size that the user will need to load it's custom fields
        uint32_t userDataSize = 0;
        if (glTFParams->pWriteExtrasCallback)
            glTFParams->pWriteExtrasCallback(&userDataSize, NULL, glTFParams->pCallbackUserData);

        PackingFunction vertexPacking[MAX_SEMANTICS] = {};
        uint32_t        vertexAttrStrides[MAX_SEMANTICS] = {};

        for (uint32_t j = 0; j < data->skins_count; ++j)
            jointCount += (uint32_t)data->skins[j].joints_count;

        // Determine index stride
        // This depends on vertex count rather than the stride specified in gltf
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

        Geometry* geom = (Geometry*)tf_calloc(1, totalGeomSize);
        ASSERT(geom);

        GeometryData* geomData = (GeometryData*)tf_calloc(1, totalGeomDataSize);
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
            uint8_t* pUserData = jointCount > 0 ? ((uint8_t*)geomData->pJointRemaps + round_up(jointCount * sizeof(uint32_t), 16))
                                                : (uint8_t*)(geomData + 1);
            geomData->pUserData = pUserData;
            geomData->userDataSize = userDataSize;
            glTFParams->pWriteExtrasCallback(&userDataSize, pUserData, glTFParams->pCallbackUserData);
        }

        // Determine vertex stride for each binding
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
                    LOGF(eERROR, "Missing attribute at index %u of pVertexLayout", attrIdx);
                    error = true;
                    continue;
                }
            }

            const uint32_t dstFormatSize = TinyImageFormat_BitSizeOfBlock((TinyImageFormat)attr->format) >> 3;
            const uint32_t srcFormatSize = (uint32_t)cgltfAttr->data->stride; //-V522

            const uint32_t thisAttrStride = dstFormatSize ? dstFormatSize : srcFormatSize;
            ASSERT(vertexAttrStrides[attr->semantic] == 0);
            vertexAttrStrides[attr->semantic] = thisAttrStride;

            // Compare vertex attrib format to the gltf attrib type
            // Select a packing function if dst format is packed version
            // Texcoords - Pack float2 to half2
            // Directions - Pack float3 to float2 to unorm2x16 (Normal, Tangent)
            // Position - No packing yet
            const hz::Format srcFormat = util_cgltf_type_to_image_format(cgltfAttr->data->type, cgltfAttr->data->component_type);
            const hz::Format dstFormat = attr->format == hz::Format::UNDEFINED ? srcFormat : attr->format;

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
                        LOGF(eERROR, "Joint size doesn't match");
                        ASSERT(false);
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

        geomData->pShadow = (GeometryData::ShadowData*)tf_calloc(1, shadowSize);
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
            // If the attribute is not present in the gltf file we just don't save anything
            if (geomData->pShadow->vertexStrides[j] == 0)
                geomData->pShadow->pAttributes[j] = nullptr;
        }

        geom->drawArgCount = drawCount;
        geom->indexType = (sizeof(uint16_t) == indexStride) ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32;
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
                jsmntok_t*  tokens = (jsmntok_t*)tf_malloc((skin->joints_count + 1) * sizeof(jsmntok_t));
                jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
                ASSERT(tokens[0].size == (int)skin->joints_count + 1);
                cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, (cgltf_float*)geomData->pInverseBindPoses,
                                             skin->joints_count * sizeof(float[16]) / sizeof(float));
                for (uint32_t r = 0; r < skin->joints_count; ++r)
                    geomData->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
                tf_free(tokens);
            }

            remapCount += (uint32_t)skin->joints_count;
        }

        // Load the tressfx specific data generated in the offline process
        if (data->asset.generator && stricmp(data->asset.generator, "tressfx") == 0)
        {
            // { "vertexCountPerStrand" : "16", "guideCountPerStrand" : "3456" }
            uint32_t    extrasSize = (uint32_t)(data->asset.extras.end_offset - data->asset.extras.start_offset);
            const char* json = data->json + data->asset.extras.start_offset;
            jsmn_parser parser = {};
            jsmntok_t   tokens[5] = {};
            jsmn_parse(&parser, (const char*)json, extrasSize, tokens, TF_ARRAY_COUNT(tokens));
            geomData->hair.vertexCountPerStrand = atoi(json + tokens[2].start);
            geomData->hair.guideCountPerStrand = atoi(json + tokens[4].start);
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
                    ASSERT(semanticIdx < TF_ARRAY_COUNT(geomData->pShadow->pAttributes));

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
                        // ResourceLoader during load
                        // TODO: Consider adding an option to make the attributes interleaved here in the AssetPipeline so that we don't
                        // have to do it in runtime.
                        //       The inconvenience in that case would be that in order to get the pShadow data we would have to unpack the
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
                // Fill draw arguments for this primitive
                /************************************************************************/
                geom->pDrawArgs[drawCount].indexCount = primitiveIndexCount;
                geom->pDrawArgs[drawCount].instanceCount = 1;
                geom->pDrawArgs[drawCount].startIndex = indexCount;
                geom->pDrawArgs[drawCount].startInstance = 0;
                // Since we already offset indices when creating the index buffer, vertex offset will be zero
                // With this approach, we can draw everything in one draw call or use the traditional draw per subset without the
                // need for changing shader code
                geom->pDrawArgs[drawCount].vertexOffset = 0;

                if (glTFParams->mProcessMeshlets)
                {
                    if (indexStride != 4)
                    {
                        LOGF(eERROR, "Cannot create meshlet when index type isn't 32-bit.");
                        error = true;
                        break;
                    }

                    if (!pos_attr || pos_attr->data->stride != 12)
                    {
                        LOGF(eERROR, "Cannot create meshlet when positions attribute is missing or layout is not float3.");
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
            void* tempStagingBuffer = tf_malloc(largestSizedAttribute);

            for (uint32_t s = SEMANTIC_POSITION + 1; s < MAX_SEMANTICS; ++s)
            {
                size_t size = (size_t)geomData->pShadow->vertexStrides[s] * geomData->pShadow->attributeCount[s];
                if (size > 0)
                {
                    memcpy(tempStagingBuffer, geomData->pShadow->pAttributes[s], size);
                }

                geomData->pShadow->pAttributes[s] = (uint8_t*)geomData->pShadow->pAttributes[s - 1] +
                                                    geomData->pShadow->vertexStrides[s - 1] * geomData->pShadow->attributeCount[s - 1];

                if (size > 0)
                {
                    memcpy(geomData->pShadow->pAttributes[s], tempStagingBuffer, size);
                }
            }

            tf_free(tempStagingBuffer);
        }

        for (uint32_t j = 0; j < TF_ARRAY_COUNT(geom->vertexStrides); ++j)
            ASSERT(geom->vertexStrides[j] == 0);

        CreateDirectoryForFile(assetParams->mRDOutput, newFileName);

        CookedGeometryFile geometryFile(assetParams->mRDOutput, newFileName);
        geometryFile.write(GEOMETRY_FILE_MAGIC_STR, sizeof(GEOMETRY_FILE_MAGIC_STR));
        geometryFile.write(&totalGeomSize, sizeof(totalGeomSize));
        geometryFile.write(geom, totalGeomSize);

        // Serialized pointers are restored from the following data blocks during load.
        GeometryData::ShadowData* pTempShadow = geomData->pShadow;
        void*                     pTempUserData = geomData->pUserData;
        geomData->pShadow = nullptr;
        geomData->pUserData = nullptr;
        geometryFile.write(&totalGeomDataSize, sizeof(totalGeomDataSize));
        geometryFile.write(geomData, totalGeomDataSize);
        geomData->pShadow = pTempShadow;
        geomData->pUserData = pTempUserData;
        geometryFile.write(&shadowSize, sizeof(shadowSize));
        geometryFile.write(geomData->pShadow, shadowSize);
        if (geom->meshlets.meshletCount)
        {
            geometryFile.write(geom->meshlets.meshlets, sizeof(*geom->meshlets.meshlets) * geom->meshlets.meshletCount);
            geometryFile.write(geom->meshlets.meshletsData, sizeof(*geom->meshlets.meshletsData) * geom->meshlets.meshletCount);
            geometryFile.write(geom->meshlets.vertices, sizeof(*geom->meshlets.vertices) * geom->meshlets.vertexCount);
            geometryFile.write(geom->meshlets.triangles, sizeof(*geom->meshlets.triangles) * geom->meshlets.triangleCount);
        }
        const bool geometryWritten = geometryFile.publish();
        if (!geometryWritten)
        {
            LOGF(eERROR, "Failed to publish geometry '%s'", newFileName);
            error = true;
        }

        if (geometryWritten && !sceneCooker.write())
        {
            LOGF(eERROR, "Failed to write scene asset files for '%s'.", fileName);
            error = true;
        }

        tf_free(geomData->pShadow);
        tf_free(geomData);

        if (geom->meshlets.meshletCount)
        {
            arrfree(geom->meshlets.meshlets);
            arrfree(geom->meshlets.meshletsData);
            arrfree(geom->meshlets.vertices);
            arrfree(geom->meshlets.triangles);
        }

        tf_free(geom);
        tf_free(pFingerprint);

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

#if defined(HORIZON_ENABLE_BUNY_COOKER)
// TODO AssetPipelineParams.mRDZipWrite ?
bool WriteZip(AssetPipelineParams* assetParams, WriteZipParams* zipParams)
{
    char** collectibles = NULL;

    for (int i = 0; i < zipParams->mFiltersCount; ++i)
    {
        char** filteredFiles = NULL;
        int    filteredFileCount = 0;

        fsGetFilesWithExtension(assetParams->mRDInput, "", zipParams->mFilters[i], &filteredFiles, &filteredFileCount);

        for (int j = 0; j < filteredFileCount; ++j)
        {
            const char* fileName = filteredFiles[j];

            size_t len = strlen(fileName);
            char*  str = (char*)tf_malloc(len + 1);
            memcpy(str, fileName, len + 1);
            arrpush(collectibles, str);
        }

        tf_free(filteredFiles);
    }

    size_t collectibleCount = arrlenu(collectibles);

    struct BunyArLibEntryCreateDesc* filesDesc = (struct BunyArLibEntryCreateDesc*)tf_malloc(collectibleCount * sizeof *filesDesc);

    for (size_t i = 0; i < collectibleCount; ++i)
    {
        struct BunyArLibEntryCreateDesc* file = filesDesc + i;

        *file = BUNYAR_LIB_FUNC_CREATE_DEFAULT_ENTRY_DESC;

        file->inputRd = assetParams->mRDInput;
        file->inputPath = collectibles[i];
    }

    BunyArLibCreateDesc archiveCreateDesc = { 0 };

    archiveCreateDesc.entryCount = collectibleCount;
    archiveCreateDesc.entries = filesDesc;
    archiveCreateDesc.verbose = 1;
    archiveCreateDesc.threadPoolSize = -1;

    bool archiveIsCreated = bunyArLibCreate(assetParams->mRDOutput, zipParams->mZipFileName, &archiveCreateDesc);

    tf_free(filesDesc);

    for (size_t i = 0; i < collectibleCount; ++i)
        tf_free(collectibles[i]);
    arrfree(collectibles);

    return !archiveIsCreated;
}

bool ZipAllAssets(AssetPipelineParams* assetParams, WriteZipParams* zipParams)
{
    // TODO: we should check that none of the existing assets have been cooked
    if (!assetParams->mSettings.force && fsFileExist(assetParams->mRDOutput, zipParams->mZipFileName))
    {
        return false;
    }

    BunyArLibCreateDesc archiveCreateDesc = { 0 };

    struct BunyArLibEntryCreateDesc entry = BUNYAR_LIB_FUNC_CREATE_DEFAULT_ENTRY_DESC;

    if (zipParams->mFiltersCount > 0)
    {
        for (int i = 0; i < zipParams->mFiltersCount; ++i)
        {
            entry.inputRd = assetParams->mRDInput;
            entry.inputPath = zipParams->mFilters[i];
            arrpush(archiveCreateDesc.entries, entry);
        }
    }
    else if (assetParams->mInDir && assetParams->mInDir[0] != '\0')
    {
        entry.inputRd = assetParams->mRDInput;
        entry.inputPath = ".";
        arrpush(archiveCreateDesc.entries, entry);
    }
    else
    {
        const char* dirs[] = {
            "Animation", "Audio", "Meshes", "Textures", "Shaders", "CompiledShaders", "Fonts", "GPUCfg", "Scripts", "Other",
        };

        entry.inputRd = assetParams->mRDInput;
        entry.optional = true;
        for (size_t i = 0; i < sizeof(dirs) / sizeof(*dirs); ++i)
        {
            entry.inputPath = dirs[i];
            arrpush(archiveCreateDesc.entries, entry);
        }
    }

    archiveCreateDesc.entryCount = arrlenu(archiveCreateDesc.entries);
    archiveCreateDesc.verbose = 1;
    archiveCreateDesc.threadPoolSize = -1;

    bool success = bunyArLibCreate(assetParams->mRDOutput, zipParams->mZipFileName, &archiveCreateDesc);

    arrfree(archiveCreateDesc.entries);

    return !success;
}
#else
bool WriteZip(AssetPipelineParams*, WriteZipParams*)
{
    LOGF(eERROR, "Archive cooking is unavailable in this AssetPipeline build.");
    return true;
}

bool ZipAllAssets(AssetPipelineParams*, WriteZipParams*)
{
    LOGF(eERROR, "Archive cooking is unavailable in this AssetPipeline build.");
    return true;
}
#endif

#if defined(HORIZON_ENABLE_ANIMATION_COOKER)
void OnDiscoverAnimation(ResourceDirectory resourceDir, const char* filename, void* pUserData)
{
    // TODO: Find beter way to discover if a .gltf mesh has external animations in other gltf files
    //		 For the time being, we assume that if this is the case, the animations are included in a "animations" subdirectory
    //		 For this reason, any subdirectory named "animations" are excluded from the discovery.
    //		 For each gltf found, we check for extra gltf animations in the "animations" subdirectory if it exists.

    const char* subdirWithAnimationsName = "animations";

    // Ignore if parent is named "animations"
    {
        char parentPath[FS_MAX_PATH] = {};
        fsGetParentPath(filename, parentPath);
        char* foundStrPntr = strstr(parentPath, subdirWithAnimationsName);
        if (foundStrPntr)
        {
            const size_t subdirWithAnimationsNameLength = strlen(subdirWithAnimationsName);
            if (foundStrPntr[subdirWithAnimationsNameLength] == '\0')
            {
                LOGF(eINFO, "Ignoring subdirectory %s.", parentPath);
                return;
            }
        }
    }

    OnDiscoverAnimationsParam* discoveredAnims = (OnDiscoverAnimationsParam*)pUserData;

    FileStream file = {};
    if (!fsOpenStreamFromPath(resourceDir, filename, FM_READ, &file))
    {
        LOGF(eERROR, "Failed to open gltf file %s", filename);
        return;
    }

    ssize_t fileSize = fsGetStreamFileSize(&file);
    void*   fileData = tf_malloc(fileSize);

    fsReadFromStream(&file, fileData, fileSize);

    cgltf_options options = {};
    cgltf_data*   data = NULL;
    options.memory_alloc = [](void* user, cgltf_size size)
    {
        UNREF_PARAM(user);
        return tf_malloc(size);
    };
    options.memory_free = [](void* user, void* ptr)
    {
        UNREF_PARAM(user);
        tf_free(ptr);
    };
    options.rd = resourceDir;
    cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
    fsCloseStream(&file);

    if (cgltf_result_success != result)
    {
        LOGF(eERROR, "Failed to parse gltf file %s with error %u", filename, (uint32_t)result);
        tf_free(fileData);
        return;
    }

    SkeletonAndAnimations skeletonAndAnims = {};
    skeletonAndAnims.mSkeletonInFile = bdynfromcstr(filename);
    skeletonAndAnims.mSkeletonOutFile = bdynfromcstr("skeleton.ozz");

    char assetPath[FS_MAX_PATH] = {};
    fsGetParentPath(filename, assetPath);

    if (data->animations_count > 0)
    {
        LOGF(eINFO, "Found Animation Mesh: %s", filename);

        SkeletonAndAnimations::AnimationFile anim = {};
        anim.mInputAnim = bdynfromcstr(filename);
        anim.mOutputAnimPath = bdynfromcstr("");
        arrpush(skeletonAndAnims.mAnimations, anim);
    }

    tf_free(fileData);
    cgltf_free(data);

    // Need to check if there are extra standalone animation assets
    DiscoverExtraAnimationsForMesh(resourceDir, assetPath, subdirWithAnimationsName, &skeletonAndAnims);

    // If we found animations, then push them in the array
    if (arrlen(skeletonAndAnims.mAnimations) > 0)
        arrpush(discoveredAnims->pSkeletonAndAnimations, skeletonAndAnims);
    else
        ReleaseSkeletonAndAnimationParams(&skeletonAndAnims, 1);
}
#else
void OnDiscoverAnimation(ResourceDirectory, const char*, void*)
{
    LOGF(eERROR, "Animation cooking is unavailable because the legacy cooker has not been migrated to the current Ozz API.");
}
#endif

int AssetPipelineRun(AssetPipelineParams* assetParams)
{
    if (assetParams->mProcessType == PROCESS_ANIMATIONS)
    {
        OnDiscoverAnimationsParam discoveredAnimations = {};

        if (assetParams->mPathMode == PROCESS_MODE_FILE)
        {
            LOGF(eINFO, "ProcessAnimations with input filepath '%s'", assetParams->mInFilePath);

            if (!fsFileExist(assetParams->mRDInput, assetParams->mInFilePath))
            {
                LOGF(eERROR, "%s doesn't exist.", assetParams->mInFilePath);
                return 1;
            }

            OnDiscoverAnimation(assetParams->mRDInput, assetParams->mInFilePath, (void*)&discoveredAnimations);
        }
        else
        {
            BeginAssetPipelineSection("DiscoverAnimations");
            DirectorySearch(assetParams->mRDInput, NULL, "gltf", OnDiscoverAnimation, (void*)&discoveredAnimations,
                            assetParams->mPathMode == PROCESS_MODE_DIRECTORY_RECURSIVE);
            EndAssetPipelineSection("DiscoverAnimations");
        }

        ProcessAnimationsParams processAnimationParams = {};
        processAnimationParams.pSkeletonAndAnims = discoveredAnimations.pSkeletonAndAnimations;
        processAnimationParams.mAnimationSettings.mSkeletonAndAnimOutRd = assetParams->mRDOutput;

        for (int32_t i = 0; i < assetParams->mFlagsCount; ++i)
        {
            if (strcmp(assetParams->mFlags[i], "--optimizetracks") == 0)
                processAnimationParams.mAnimationSettings.mOptimizeTracks = true;
        }

        BeginAssetPipelineSection("ProcessAnimations");
        const bool result = ProcessAnimations(assetParams, &processAnimationParams);
        ReleaseSkeletonAndAnimationParams(discoveredAnimations.pSkeletonAndAnimations);
        EndAssetPipelineSection("ProcessAnimations");
        return result;
    }

    if (assetParams->mProcessType == PROCESS_TFX)
    {
        ProcessTressFXParams tfxParams;
        BeginAssetPipelineSection("ProcessTFX");
        bool result = ProcessTFX(assetParams, &tfxParams);
        EndAssetPipelineSection("ProcessTFX");
        return result;
    }

    if (assetParams->mProcessType == PROCESS_GLTF)
    {
        bool error = false;
        bool isHair = false;

        MeshOptimizerFlags meshOptimizerFlags = MESH_OPTIMIZATION_FLAG_OFF;

        bool processMeshlets = false;
        /// Recommended number of vertices and triangles are from
        /// https://gpuopen.com/learn/mesh_shaders/mesh_shaders-optimization_and_best_practices/
        int  numMeshletVertices = 128;
        int  numMeshletTriangles = 256;

        for (int32_t i = 0; i < assetParams->mFlagsCount; ++i)
        {
            if (strcmp(assetParams->mFlags[i], "--hair") == 0)
                isHair = true;
            else if (strcmp(assetParams->mFlags[i], "--optimize") == 0)
                meshOptimizerFlags |= MESH_OPTIMIZATION_FLAG_ALL;
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
                    LOGF(eERROR, "Maximum number of vertices in a meshlet should be larger than 0 and smaller than 256.");
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
                    LOGF(eERROR, "Maximum number of triangles in a meshlet should be larger than 0 and smaller than 513.");
                    error = true;
                }
            }
        }

        if (error)
            return 1;

        VertexLayout vertexLayout = {};

        // Hair mesh for UT06
        if (isHair)
        {
            vertexLayout.attribCount = 7;
            vertexLayout.attribs[0].semantic = SEMANTIC_POSITION;
            vertexLayout.attribs[0].binding = 0;
            vertexLayout.attribs[1].semantic = SEMANTIC_TANGENT;
            vertexLayout.attribs[1].binding = 1;
            vertexLayout.attribs[2].semantic = SEMANTIC_TEXCOORD0;
            vertexLayout.attribs[2].binding = 2;
            vertexLayout.attribs[3].semantic = SEMANTIC_TEXCOORD2;
            vertexLayout.attribs[3].binding = 4;
            vertexLayout.attribs[4].semantic = SEMANTIC_TEXCOORD3;
            vertexLayout.attribs[4].binding = 5;
            vertexLayout.attribs[5].semantic = SEMANTIC_TEXCOORD6;
            vertexLayout.attribs[5].binding = 8;
            vertexLayout.attribs[6].semantic = SEMANTIC_TEXCOORD7;
            vertexLayout.attribs[6].binding = 9;
        }
        else
        {
            vertexLayout.attribCount = 6;
            vertexLayout.attribs[0].semantic = SEMANTIC_POSITION;
            vertexLayout.attribs[0].format = hz::Format::R32G32B32_SFLOAT;
            vertexLayout.attribs[0].binding = 0;
            vertexLayout.attribs[0].location = 0;
            vertexLayout.attribs[0].offset = 0;
            vertexLayout.attribs[1].semantic = SEMANTIC_NORMAL;
            vertexLayout.attribs[1].format = hz::Format::R32_UINT;
            vertexLayout.attribs[1].binding = 1;
            vertexLayout.attribs[1].location = 1;
            vertexLayout.attribs[1].offset = 0;
            vertexLayout.attribs[2].semantic = SEMANTIC_TANGENT;
            vertexLayout.attribs[2].format = hz::Format::R32_UINT;
            vertexLayout.attribs[2].binding = 2;
            vertexLayout.attribs[2].location = 2;
            vertexLayout.attribs[2].offset = 0;
            vertexLayout.attribs[3].semantic = SEMANTIC_TEXCOORD0;
            vertexLayout.attribs[3].format = hz::Format::R32_UINT;
            vertexLayout.attribs[3].binding = 3;
            vertexLayout.attribs[3].location = 3;
            vertexLayout.attribs[3].offset = 0;
            vertexLayout.attribs[4].semantic = SEMANTIC_JOINTS;
            vertexLayout.attribs[4].format = hz::Format::R16G16B16A16_UINT;
            vertexLayout.attribs[4].binding = 4;
            vertexLayout.attribs[4].location = 4;
            vertexLayout.attribs[4].offset = 0;
            vertexLayout.attribs[5].semantic = SEMANTIC_WEIGHTS;
            vertexLayout.attribs[5].format = hz::Format::R32G32B32A32_SFLOAT;
            vertexLayout.attribs[5].binding = 5;
            vertexLayout.attribs[5].location = 5;
            vertexLayout.attribs[5].offset = 0;
        }

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

    if (assetParams->mProcessType == PROCESS_TEXTURES)
    {
        ProcessTexturesParams texturesParams = {};

        texturesParams.mInExt = assetParams->mInExt;
        texturesParams.mCompression = COMPRESSION_NONE;
        texturesParams.mContainer = CONTAINER_DDS; // default container
        texturesParams.mGenerateMipmaps = MIPMAP_NONE;
        texturesParams.mInputLinearColorSpace = false;

        bool error = false;
        for (int i = 0; i < assetParams->mFlagsCount; i++)
        {
            const char* flag = assetParams->mFlags[i];

            if (STRCMP(flag, "--in-all"))
            {
                if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
                {
                    LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
                    error = true;
                }

                texturesParams.mInExt = "*";
            }
            else if (STRCMP(flag, "--in-jpg"))
            {
                if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
                {
                    LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
                    error = true;
                }

                texturesParams.mInExt = "jpg";
            }
            else if (STRCMP(flag, "--in-png"))
            {
                if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
                {
                    LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
                    error = true;
                }

                texturesParams.mInExt = "png";
            }
            else if (STRCMP(flag, "--in-ktx"))
            {
                if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
                {
                    LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
                    error = true;
                }

                texturesParams.mInExt = "ktx";
            }
            else if (STRCMP(flag, "--in-dds"))
            {
                if (texturesParams.mInExt && texturesParams.mInExt[0] != '\0')
                {
                    LOGF(eERROR, "Already set input format to %s.", texturesParams.mInExt);
                    error = true;
                }

                texturesParams.mInExt = "dds";
            }
            else if (STRCMP(flag, "--out-ktx"))
            {
                texturesParams.mContainer = CONTAINER_KTX;
            }
            else if (STRCMP(flag, "--out-dds"))
            {
                texturesParams.mContainer = CONTAINER_DDS;
            }
            else if (STRCMP(flag, "--out-gnf-orbis"))
            {
#ifdef PROSPERO_GNF
                texturesParams.mContainer = CONTAINER_GNF_ORBIS;
#else
                LOGF(eERROR, "AssetPipeline wasn't compiled with Pospero GNF texture conversion support.");
#endif
            }
            else if (STRCMP(flag, "--out-gnf-prospero"))
            {
#ifdef PROSPERO_GNF
                texturesParams.mContainer = CONTAINER_GNF_PROSPERO;
#else
                LOGF(eERROR, "AssetPipeline wasn't compiled with Prospero GNF texture conversion support.");
#endif
            }
            else if (STRCMP(flag, "--out-xdds"))
            {
#ifdef XBOX_SCARLETT_DDS
                texturesParams.mContainer = CONTAINER_SCARLETT_DDS;
#else
                LOGF(eERROR, "AssetPipeline wasn't compiled with Xbox DDS texture conversion support.");
#endif
            }
            else if (STRCMP(flag, "--astc"))
            {
                texturesParams.mCompression = COMPRESSION_ASTC;
            }
            else if (STRCMP(flag, "--astc4x4"))
            {
                texturesParams.mCompression = COMPRESSION_ASTC;
                texturesParams.mOverrideASTC = ASTC_4x4;
            }
            else if (STRCMP(flag, "--astc4x4-slow"))
            {
                texturesParams.mCompression = COMPRESSION_ASTC;
                texturesParams.mOverrideASTC = ASTC_4x4_SLOW;
            }
            else if (STRCMP(flag, "--astc8x8"))
            {
                texturesParams.mCompression = COMPRESSION_ASTC;
                texturesParams.mOverrideASTC = ASTC_8x8;
            }
            else if (STRCMP(flag, "--astc8x8-slow"))
            {
                texturesParams.mCompression = COMPRESSION_ASTC;
                texturesParams.mOverrideASTC = ASTC_8x8_SLOW;
            }
            else if (STRCMP(flag, "--bc"))
            {
                texturesParams.mCompression = COMPRESSION_BC;
            }
            else if (STRCMP(flag, "--bc1"))
            {
                texturesParams.mCompression = COMPRESSION_BC;
                texturesParams.mOverrideBC = DXT_BC1;
            }
            else if (STRCMP(flag, "--bc3"))
            {
                texturesParams.mCompression = COMPRESSION_BC;
                texturesParams.mOverrideBC = DXT_BC3;
            }
            else if (STRCMP(flag, "--bc4"))
            {
                texturesParams.mCompression = COMPRESSION_BC;
                texturesParams.mOverrideBC = DXT_BC4;
            }
            else if (STRCMP(flag, "--bc5"))
            {
                texturesParams.mCompression = COMPRESSION_BC;
                texturesParams.mOverrideBC = DXT_BC5;
            }
            else if (STRCMP(flag, "--bc7"))
            {
                texturesParams.mCompression = COMPRESSION_BC;
                texturesParams.mOverrideBC = DXT_BC7;
            }
            else if (STRCMP(flag, "--genmips"))
            {
                texturesParams.mGenerateMipmaps = MIPMAP_DEFAULT;
            }
            else if (STRCMP(flag, "--vmf"))
            {
                texturesParams.pRoughnessFilePath = assetParams->mFlags[++i];
            }
            else if (STRCMP(flag, "--in-linear"))
            {
                texturesParams.mInputLinearColorSpace = true;
            }
            else
            {
                LOGF(eERROR, "Unrecognized flag %s.", flag);

                error = true;
            }
        }

        if (!texturesParams.mInExt)
        {
            LOGF(eERROR, "Texture input format isn't set.");
            error = true;
        }

        if (error)
            return 1;

        BeginAssetPipelineSection("ProcessTextures");
        bool result = ProcessTextures(assetParams, &texturesParams);
        EndAssetPipelineSection("ProcessTextures");
        return result;
    }

    if (assetParams->mProcessType == PROCESS_WRITE_ZIP || assetParams->mProcessType == PROCESS_WRITE_ZIP_ALL)
    {
        WriteZipParams zipParams;

        zipParams.mZipFileName = assetParams->mProcessType == PROCESS_WRITE_ZIP ? "Output.zip" : "Assets.zip";
        zipParams.mFiltersCount = 0;

        for (int i = 0; i < assetParams->mFlagsCount; i++)
        {
            const char* flag = assetParams->mFlags[i];

            if (STRCMP(flag, "--filter"))
            {
                i++;
                zipParams.mFiltersCount = 0;
                for (; i < assetParams->mFlagsCount; i++)
                {
                    const char* filter = assetParams->mFlags[i];

                    zipParams.mFilters[zipParams.mFiltersCount++] = filter;

                    // next argument is a flag, not an extension
                    if (i + 1 == assetParams->mFlagsCount || assetParams->mFlags[i + 1][0] == '-')
                        break;
                }
            }
            else if (STRCMP(flag, "--name"))
            {
                i++;
                zipParams.mZipFileName = assetParams->mFlags[i];
            }
            else
            {
                LOGF(eERROR, "Unrecognized flag %s.", flag);
            }
        }

        BeginAssetPipelineSection("ProcessZipAssets");
        bool result = false;
        if (assetParams->mProcessType == PROCESS_WRITE_ZIP)
        {
            result = WriteZip(assetParams, &zipParams);
        }
        else
        {
            result = ZipAllAssets(assetParams, &zipParams);
        }
        EndAssetPipelineSection("ProcessZipAssets");
        return result;
    }

    // if reached, the used processed type wasn't included
    ASSERT(false);
    return 0;
}

bool ensureSceneGltfCooked(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory)
{
    if (!pSourceFile || !pSourceFile[0] || sourceDirectory == outputDirectory ||
        strcmp(fsGetResourceDirectory(sourceDirectory), fsGetResourceDirectory(outputDirectory)) == 0)
    {
        LOGF(eERROR, "glTF cooking requires separate source and output directories");
        return false;
    }
    AssetPipelineParams params = {};
    params.mInFilePath = pSourceFile;
    params.mInExt = "gltf";
    params.mProcessType = PROCESS_GLTF;
    params.mPathMode = PROCESS_MODE_FILE;
    params.mRDInput = sourceDirectory;
    params.mRDOutput = outputDirectory;
    if (!fsCreateDirectory(outputDirectory, "", true) || AssetPipelineRun(&params) != 0)
    {
        LOGF(eERROR, "Failed to cook %s", pSourceFile);
        return false;
    }
    return true;
}
