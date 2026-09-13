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
    SceneAssetErrorCode code;
    char                message[SCENE_ASSET_ERROR_MESSAGE_CAPACITY];
} SceneAssetError;

typedef struct SceneAssetMaterialConvention
{
    char baseColor[SCENE_ASSET_METADATA_CAPACITY];
    char specular[SCENE_ASSET_METADATA_CAPACITY];
    char normal[SCENE_ASSET_METADATA_CAPACITY];
    char emissive[SCENE_ASSET_METADATA_CAPACITY];
} SceneAssetMaterialConvention;

typedef struct SceneAssetTextureManifest
{
    char path[FS_MAX_PATH];
    bool srgb;
} SceneAssetTextureManifest;

typedef struct SceneAssetMaterialManifest
{
    char    name[SCENE_ASSET_NAME_CAPACITY];
    int32_t baseColorTexture;
    int32_t normalTexture;
    int32_t metallicRoughnessTexture;
    int32_t emissiveTexture;
    float   baseColorFactor[4];
    float   metallicFactor;
    float   roughnessFactor;
    float   emissiveFactor[3];
} SceneAssetMaterialManifest;

typedef struct SceneAssetGpuMaterial
{
    uint32_t baseColorTexture;
    uint32_t normalTexture;
    uint32_t metallicRoughnessTexture;
    uint32_t emissiveTexture;
    float    baseColorFactor[4];
    float    emissiveFactor[3];
    float    metallicFactor;
    float    roughnessFactor;
    float    padding[3];
} SceneAssetGpuMaterial;

typedef struct SceneAssetManifest
{
    uint32_t                     version;
    char                         contentHash[SCENE_ASSET_CONTENT_HASH_CAPACITY];
    uint32_t                     dependencyCount;
    char                         dependencies[SCENE_ASSET_MAX_DEPENDENCIES][FS_MAX_PATH];
    char                         defaultScene[SCENE_ASSET_NAME_CAPACITY];
    char                         geometry[FS_MAX_PATH];
    char                         sourceGltf[FS_MAX_PATH];
    char                         environment[FS_MAX_PATH];
    uint32_t                     textureDirectoryCount;
    char                         textureDirectories[SCENE_ASSET_MAX_TEXTURE_DIRECTORIES][FS_MAX_PATH];
    uint32_t                     textureCount;
    SceneAssetTextureManifest    textures[SCENE_ASSET_MAX_TEXTURES];
    uint32_t                     materialCount;
    SceneAssetMaterialManifest   materials[SCENE_ASSET_MAX_MATERIALS];
    SceneAssetMaterialConvention materialConvention;
} SceneAssetManifest;

struct SceneAssetSlot;
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
    uint32_t magic;
    uint32_t instanceCount;
    float    boundsMin[3];
    float    boundsMax[3];
    float    cameraWorld[16];
    float    cameraYFov;
    uint32_t hasCamera;
    uint32_t padding[2];
};

struct SceneAssetInstance
{
    float    world[16]; // Column-major, including all parent node transforms.
    uint32_t drawIndex;
    uint32_t materialIndex;
    float    alphaCutoff;
    uint32_t padding;
};

typedef struct SceneAssetHandle
{
    uint32_t index;
    uint32_t generation;
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
    uint32_t                    capacity;
    // Must outlive the manager and all GPU work using its scenes.
    hz::RenderContext*          pContext;
    SceneAssetResourceCallbacks callbacks;
    void*                       pUserData;
    // Optional tool integration. Validate the cache and cook missing/outdated outputs
    // synchronously before starting GPU loads. Does not initialize global services.
    bool (*pEnsureGltfCooked)(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory,
                              SceneAssetError* pError);
} SceneManagerDesc;

class SceneManager
{
public:
    SceneManager(const SceneManagerDesc& desc);
    ~SceneManager();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void update();

    // Standalone geometry only: pGeometryBuffer must be null. Optional ppGeometryData
    // remains caller-owned CPU data; the manager owns all GPU outputs.
    SceneAssetHandle requestFromManifest(const SceneAssetManifest* pManifest, const GeometryLoadDesc* pGeometryLoadDesc);
    SceneAssetHandle requestFromManifestFile(ResourceDirectory resourceDirectory, const char* pManifestFileName,
                                             const GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError);
    // Filename is relative to sourceDirectory. Without a cooker, loads existing cooked
    // manifests only. RD_MESHES/RD_TEXTURES must already point to cooked/source roots.
    SceneAssetHandle requestFromGltf(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory,
                                     const GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError);
    // Wait for GPU users before release. Pending loader work is retired asynchronously.
    bool             release(SceneAssetHandle handle);

    SceneAssetStatus             getStatus(SceneAssetHandle handle) const;
    // Include Scene/SceneGeometry.h to access geometry buffers. The reference expires on release.
    const SceneGeometry*         getGeometry(SceneAssetHandle handle) const;
    uint32_t                     getTextureCount(SceneAssetHandle handle) const;
    const hz::GPUTexture*        getTexture(SceneAssetHandle handle, uint32_t textureIndex) const;
    const hz::GPUBuffer*         getMaterialBuffer(SceneAssetHandle handle) const;
    uint32_t                     getMaterialCount(SceneAssetHandle handle) const;
    const SceneAssetGpuMaterial* getGpuMaterials(SceneAssetHandle handle) const;
    const SceneAssetManifest*    getManifest(SceneAssetHandle handle) const;
    bool                         isGeometryResident(SceneAssetHandle handle) const;
    bool                         isMaterialResident(SceneAssetHandle handle) const;
    bool                         isTextureResident(SceneAssetHandle handle, uint32_t textureIndex) const;
    bool updateBindlessTextures(Renderer* pRenderer, uint32_t setIndex, DescriptorSet* pDescriptorSet, const char* pBindingName,
                                SceneAssetHandle handle) const;

private:
    SceneAssetSlot*       findSlot(SceneAssetHandle handle);
    const SceneAssetSlot* findSlot(SceneAssetHandle handle) const;
    void                  destroySlot(SceneAssetSlot* pSlot);

    SceneAssetSlot*             pSlots = nullptr;
    hz::RenderContext*          pContext = nullptr;
    SceneAssetResourceCallbacks callbacks = {};
    void*                       pUserData = nullptr;
    bool (*pEnsureGltfCooked)(ResourceDirectory, const char*, ResourceDirectory, SceneAssetError*) = nullptr;
};

bool parseSceneAssetManifest(const char* pJson, size_t jsonSize, SceneAssetManifest* pManifest, SceneAssetError* pError);
bool loadSceneAssetManifest(ResourceDirectory resourceDirectory, const char* pFileName, SceneAssetManifest* pManifest,
                            SceneAssetError* pError);
bool prepareSceneAssetGeometryLoadDesc(const SceneAssetManifest* pManifest, const GeometryLoadDesc* pSource,
                                       GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError);

bool isSceneAssetHandleValid(SceneAssetHandle handle);
