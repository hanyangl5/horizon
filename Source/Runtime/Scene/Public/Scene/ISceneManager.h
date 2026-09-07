/*
 * Copyright (c) 2026 Horizon
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Core/IFileSystem.h"
#include "Resources/IResourceLoader.h"

#define SCENE_ASSET_MANIFEST_VERSION        1u
#define SCENE_ASSET_NAME_CAPACITY           128u
#define SCENE_ASSET_METADATA_CAPACITY       128u
#define SCENE_ASSET_MAX_TEXTURE_DIRECTORIES 8u
#define SCENE_ASSET_MAX_TEXTURES            512u
#define SCENE_ASSET_MAX_MATERIALS           256u
#define SCENE_ASSET_MAX_DEPENDENCIES        512u
#define SCENE_ASSET_CONTENT_HASH_CAPACITY   32u
#define SCENE_ASSET_ERROR_MESSAGE_CAPACITY  256u

typedef enum SceneAssetErrorCode
{
    SCENE_ASSET_ERROR_NONE = 0,
    SCENE_ASSET_ERROR_INVALID_ARGUMENT,
    SCENE_ASSET_ERROR_INVALID_JSON,
    SCENE_ASSET_ERROR_UNSUPPORTED_VERSION,
    SCENE_ASSET_ERROR_MISSING_FIELD,
    SCENE_ASSET_ERROR_INVALID_FIELD,
    SCENE_ASSET_ERROR_INVALID_PATH,
    SCENE_ASSET_ERROR_CAPACITY_EXCEEDED,
    SCENE_ASSET_ERROR_IO,
} SceneAssetErrorCode;

typedef struct SceneAssetError
{
    SceneAssetErrorCode mCode;
    char                mMessage[SCENE_ASSET_ERROR_MESSAGE_CAPACITY];
} SceneAssetError;

typedef struct SceneAssetMaterialConvention
{
    char mBaseColor[SCENE_ASSET_METADATA_CAPACITY];
    char mSpecular[SCENE_ASSET_METADATA_CAPACITY];
    char mNormal[SCENE_ASSET_METADATA_CAPACITY];
    char mEmissive[SCENE_ASSET_METADATA_CAPACITY];
} SceneAssetMaterialConvention;

typedef struct SceneAssetTextureManifest
{
    char mPath[FS_MAX_PATH];
    bool mSrgb;
} SceneAssetTextureManifest;

typedef struct SceneAssetMaterialManifest
{
    char    mName[SCENE_ASSET_NAME_CAPACITY];
    int32_t mBaseColorTexture;
    int32_t mNormalTexture;
    int32_t mMetallicRoughnessTexture;
    int32_t mEmissiveTexture;
    float   mBaseColorFactor[4];
    float   mMetallicFactor;
    float   mRoughnessFactor;
    float   mEmissiveFactor[3];
} SceneAssetMaterialManifest;

typedef struct SceneAssetGpuMaterial
{
    uint32_t mBaseColorTexture;
    uint32_t mNormalTexture;
    uint32_t mMetallicRoughnessTexture;
    uint32_t mEmissiveTexture;
    float    mBaseColorFactor[4];
    float    mEmissiveFactor[3];
    float    mMetallicFactor;
    float    mRoughnessFactor;
    float    mPadding[3];
} SceneAssetGpuMaterial;

typedef struct SceneAssetManifest
{
    uint32_t                     mVersion;
    char                         mContentHash[SCENE_ASSET_CONTENT_HASH_CAPACITY];
    uint32_t                     mDependencyCount;
    char                         mDependencies[SCENE_ASSET_MAX_DEPENDENCIES][FS_MAX_PATH];
    char                         mDefaultScene[SCENE_ASSET_NAME_CAPACITY];
    char                         mGeometry[FS_MAX_PATH];
    char                         mSourceGltf[FS_MAX_PATH];
    char                         mEnvironment[FS_MAX_PATH];
    uint32_t                     mTextureDirectoryCount;
    char                         mTextureDirectories[SCENE_ASSET_MAX_TEXTURE_DIRECTORIES][FS_MAX_PATH];
    uint32_t                     mTextureCount;
    SceneAssetTextureManifest    mTextures[SCENE_ASSET_MAX_TEXTURES];
    uint32_t                     mMaterialCount;
    SceneAssetMaterialManifest   mMaterials[SCENE_ASSET_MAX_MATERIALS];
    SceneAssetMaterialConvention mMaterialConvention;
} SceneAssetManifest;

struct SceneManager;
struct SceneGeometry;
namespace hz
{
class RenderContext;
class GPUBuffer;
class GPUTexture;
} // namespace hz

// Stored in GeometryData::pUserData by the glTF cooker when no custom extras callback is supplied.
#define SCENE_ASSET_GEOMETRY_MAGIC 0x53434E31u
struct SceneAssetGeometryHeader
{
    uint32_t mMagic;
    uint32_t mInstanceCount;
    float    mBoundsMin[3];
    float    mBoundsMax[3];
    float    mCameraWorld[16];
    float    mCameraYFov;
    uint32_t mHasCamera;
    uint32_t mPadding[2];
};

struct SceneAssetInstance
{
    float    mWorld[16]; // Column-major, including all parent node transforms.
    uint32_t mDrawIndex;
    uint32_t mMaterialIndex;
    float    mAlphaCutoff;
    uint32_t mPadding;
};

typedef struct SceneAssetHandle
{
    uint32_t mIndex;
    uint32_t mGeneration;
} SceneAssetHandle;

typedef enum SceneAssetStatus
{
    SCENE_ASSET_STATUS_INVALID = 0,
    SCENE_ASSET_STATUS_LOADING,
    SCENE_ASSET_STATUS_READY,
    SCENE_ASSET_STATUS_FAILED,
} SceneAssetStatus;

typedef void (*SceneAssetLoadGeometryFn)(GeometryLoadDesc* pDesc, SyncToken* pToken, void* pUserData);
typedef void (*SceneAssetLoadTextureFn)(TextureLoadDesc* pDesc, SyncToken* pToken, void* pUserData);
typedef void (*SceneAssetLoadBufferFn)(BufferLoadDesc* pDesc, SyncToken* pToken, void* pUserData);
typedef bool (*SceneAssetIsTokenCompletedFn)(const SyncToken* pToken, void* pUserData);
typedef void (*SceneAssetWaitForTokenFn)(const SyncToken* pToken, void* pUserData);
typedef void (*SceneAssetRemoveResourceFn)(void* pUserData, void* pResource);

typedef struct SceneAssetResourceCallbacks
{
    // Custom loaders must return resources from pContext's device. Successful uploads
    // transfer GPU ownership to the scene; removal callbacks clean up unadopted outputs
    // and Geometry metadata (whose buffer pointers are null after adoption).
    SceneAssetLoadGeometryFn     pLoadGeometry;
    SceneAssetLoadTextureFn      pLoadTexture;
    SceneAssetLoadBufferFn       pLoadBuffer;
    SceneAssetIsTokenCompletedFn pIsTokenCompleted;
    SceneAssetWaitForTokenFn     pWaitForToken;
    SceneAssetRemoveResourceFn   pRemoveGeometry;
    SceneAssetRemoveResourceFn   pRemoveTexture;
    SceneAssetRemoveResourceFn   pRemoveBuffer;
} SceneAssetResourceCallbacks;

typedef struct SceneManagerDesc
{
    uint32_t                    mCapacity;
    // Must outlive the manager and all GPU work using its scenes.
    hz::RenderContext*          pContext;
    SceneAssetResourceCallbacks mCallbacks;
    void*                       pUserData;
    // Optional tool integration. Validate the cache and cook missing/outdated outputs
    // synchronously before starting GPU loads. Does not initialize global services.
    bool (*pEnsureGltfCooked)(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory,
                              SceneAssetError* pError);
} SceneManagerDesc;

bool parseSceneAssetManifest(const char* pJson, size_t jsonSize, SceneAssetManifest* pManifest, SceneAssetError* pError);
bool loadSceneAssetManifest(ResourceDirectory resourceDirectory, const char* pFileName, SceneAssetManifest* pManifest,
                            SceneAssetError* pError);
bool prepareSceneAssetGeometryLoadDesc(const SceneAssetManifest* pManifest, const GeometryLoadDesc* pSource,
                                       GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError);

bool initSceneManager(const SceneManagerDesc* pDesc, SceneManager** ppSystem);
void exitSceneManager(SceneManager* pSystem);
void updateSceneManager(SceneManager* pSystem);

// Standalone geometry only: pGeometryBuffer must be null. Optional ppGeometryData
// remains caller-owned CPU data; the manager owns all GPU outputs.
SceneAssetHandle             requestSceneAssetFromManifest(SceneManager* pSystem, const SceneAssetManifest* pManifest,
                                                           const GeometryLoadDesc* pGeometryLoadDesc);
SceneAssetHandle             requestSceneAsset(SceneManager* pSystem, ResourceDirectory resourceDirectory, const char* pManifestFileName,
                                               const GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError);
// Filename is relative to sourceDirectory. Without a cooker, loads existing cooked
// manifests only. RD_MESHES/RD_TEXTURES must already point to cooked/source roots.
SceneAssetHandle             requestSceneAssetFromGltf(SceneManager* pSystem, ResourceDirectory sourceDirectory, const char* pSourceFile,
                                                       ResourceDirectory outputDirectory, const GeometryLoadDesc* pGeometryLoadDesc,
                                                       SceneAssetError* pError);
// Wait for GPU users before release. Pending loader work is retired asynchronously.
bool                         releaseSceneAsset(SceneManager* pSystem, SceneAssetHandle handle);
bool                         isSceneAssetHandleValid(SceneAssetHandle handle);
SceneAssetStatus             getSceneAssetStatus(const SceneManager* pSystem, SceneAssetHandle handle);
// Include Scene/SceneGeometry.h to access geometry buffers. Getters return null until
// the requested resource is uploaded, or for invalid handles. References expire on release.
const SceneGeometry*         getSceneAssetGeometry(const SceneManager* pSystem, SceneAssetHandle handle);
uint32_t                     getSceneAssetTextureCount(const SceneManager* pSystem, SceneAssetHandle handle);
const hz::GPUTexture*        getSceneAssetTexture(const SceneManager* pSystem, SceneAssetHandle handle, uint32_t textureIndex);
const hz::GPUBuffer*         getSceneAssetMaterialBuffer(const SceneManager* pSystem, SceneAssetHandle handle);
uint32_t                     getSceneAssetMaterialCount(const SceneManager* pSystem, SceneAssetHandle handle);
const SceneAssetGpuMaterial* getSceneAssetGpuMaterials(const SceneManager* pSystem, SceneAssetHandle handle);
const SceneAssetManifest*    getSceneAssetManifest(const SceneManager* pSystem, SceneAssetHandle handle);
bool                         isSceneAssetGeometryResident(const SceneManager* pSystem, SceneAssetHandle handle);
bool                         isSceneAssetMaterialResident(const SceneManager* pSystem, SceneAssetHandle handle);
bool                         isSceneAssetTextureResident(const SceneManager* pSystem, SceneAssetHandle handle, uint32_t textureIndex);
bool updateSceneAssetBindlessTextures(Renderer* pRenderer, uint32_t setIndex, DescriptorSet* pDescriptorSet, const char* pBindingName,
                                      const SceneManager* pSystem, SceneAssetHandle handle);
