/*
 * Copyright (c) 2026 Horizon
 */

#include "Scene/ISceneManager.h"
#include "Scene/SceneGeometry.h"
#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <ThirdParty/cJSON/cJSON.h>
#include <ThirdParty/stb/stb_ds.h>

#include "Core/IMemory.h"

static const size_t kMaxSceneAssetManifestSize = 1024u * 1024u;

static bool failSceneAsset(SceneAssetError* pError, SceneAssetErrorCode code, const char* pMessage)
{
    if (pError)
    {
        pError->code = code;
        snprintf(pError->message, sizeof(pError->message), "%s", pMessage);
    }
    return false;
}

static bool copySceneAssetString(const cJSON* pObject, const char* pName, char* pOutput, size_t outputCapacity, bool required,
                                 SceneAssetError* pError)
{
    const cJSON* pValue = cJSON_GetObjectItemCaseSensitive(pObject, pName);
    if (!pValue)
    {
        if (!required)
            return true;
        return failSceneAsset(pError, SCENE_ASSET_ERROR_MISSING_FIELD, pName);
    }
    if (!cJSON_IsString(pValue) || !pValue->valuestring || !pValue->valuestring[0])
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, pName);

    const size_t length = strlen(pValue->valuestring);
    if (length >= outputCapacity)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_CAPACITY_EXCEEDED, pName);

    memcpy(pOutput, pValue->valuestring, length + 1);
    return true;
}

static bool isSceneAssetRelativePath(const char* pPath)
{
    if (!pPath || !pPath[0] || pPath[0] == '/' || pPath[0] == '\\')
        return false;
    if (isalpha((unsigned char)pPath[0]) && pPath[1] == ':')
        return false;

    const char* pSegment = pPath;
    for (const char* pCursor = pPath;; ++pCursor)
    {
        if (*pCursor == '\\' || *pCursor == ':')
            return false;
        if (*pCursor == '/' || *pCursor == '\0')
        {
            if (pCursor - pSegment == 2 && pSegment[0] == '.' && pSegment[1] == '.')
                return false;
            if (*pCursor == '\0')
                break;
            pSegment = pCursor + 1;
        }
    }
    return true;
}

static bool hasSceneAssetTextureExtension(const char* pPath, const char* pExpectedExtension)
{
    const char* pExtension = strrchr(pPath, '.');
    if (!pExtension)
        return false;
    for (; *pExtension && *pExpectedExtension; ++pExtension, ++pExpectedExtension)
    {
        if (tolower((unsigned char)*pExtension) != tolower((unsigned char)*pExpectedExtension))
            return false;
    }
    return !*pExtension && !*pExpectedExtension;
}

static bool isSceneAssetTexturePath(const char* pPath)
{
    return hasSceneAssetTextureExtension(pPath, ".dds") || hasSceneAssetTextureExtension(pPath, ".ktx");
}

static TextureContainerType getSceneAssetTextureContainer(const char* pPath)
{
    return hasSceneAssetTextureExtension(pPath, ".ktx") ? TEXTURE_CONTAINER_KTX : TEXTURE_CONTAINER_DDS;
}

static bool copySceneAssetPath(const cJSON* pObject, const char* pName, char* pOutput, bool required, SceneAssetError* pError)
{
    if (!copySceneAssetString(pObject, pName, pOutput, FS_MAX_PATH, required, pError))
        return false;
    if (!pOutput[0])
        return true;
    if (!isSceneAssetRelativePath(pOutput))
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_PATH, pName);
    return true;
}

static bool copySceneAssetFloatArray(const cJSON* pObject, const char* pName, float* pOutput, uint32_t count, SceneAssetError* pError)
{
    const cJSON* pArray = cJSON_GetObjectItemCaseSensitive(pObject, pName);
    if (!pArray)
        return true;
    if (!cJSON_IsArray(pArray) || cJSON_GetArraySize(pArray) != (int)count)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, pName);
    for (uint32_t i = 0; i < count; ++i)
    {
        const cJSON* pValue = cJSON_GetArrayItem(pArray, (int)i);
        if (!cJSON_IsNumber(pValue))
            return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, pName);
        pOutput[i] = (float)pValue->valuedouble;
    }
    return true;
}

static bool copySceneAssetTextureIndex(const cJSON* pMaterial, const char* pName, uint32_t textureCount, int32_t* pOutput,
                                       SceneAssetError* pError)
{
    const cJSON* pValue = cJSON_GetObjectItemCaseSensitive(pMaterial, pName);
    if (!pValue)
        return true;
    if (!cJSON_IsNumber(pValue) || pValue->valuedouble != (double)pValue->valueint || pValue->valueint < -1 ||
        pValue->valueint >= (int)textureCount)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, pName);
    *pOutput = pValue->valueint;
    return true;
}

static bool isSceneAssetContentHash(const char* pHash)
{
    static const char prefix[] = "fnv1a64:";
    if (!pHash || strncmp(pHash, prefix, sizeof(prefix) - 1) != 0 || strlen(pHash) != sizeof(prefix) - 1 + 16)
        return false;
    for (const char* pDigit = pHash + sizeof(prefix) - 1; *pDigit; ++pDigit)
    {
        if (!isxdigit((unsigned char)*pDigit))
            return false;
    }
    return true;
}

bool parseSceneAssetManifest(const char* pJson, size_t jsonSize, SceneAssetManifest* pManifest, SceneAssetError* pError)
{
    if (pError)
        *pError = {};
    if (!pManifest)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_ARGUMENT, "pManifest");
    memset(pManifest, 0, sizeof(*pManifest));
    if (!pJson || !jsonSize)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_ARGUMENT, "pJson");

    const char* pParseEnd = nullptr;
    cJSON*      pRoot = cJSON_ParseWithLengthOpts(pJson, jsonSize, &pParseEnd, false);
    if (!pRoot)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_JSON, "manifest is not valid JSON");

    while (pParseEnd && pParseEnd < pJson + jsonSize && isspace((unsigned char)*pParseEnd))
        ++pParseEnd;
    if (!pParseEnd || pParseEnd != pJson + jsonSize || !cJSON_IsObject(pRoot))
    {
        cJSON_Delete(pRoot);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_JSON, "manifest root must be one JSON object");
    }

    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(pRoot, "version");
    if (!cJSON_IsNumber(pVersion))
    {
        cJSON_Delete(pRoot);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_MISSING_FIELD, "version");
    }
    if (pVersion->valuedouble != (double)SCENE_ASSET_MANIFEST_VERSION)
    {
        cJSON_Delete(pRoot);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_UNSUPPORTED_VERSION, "unsupported manifest version");
    }
    pManifest->version = SCENE_ASSET_MANIFEST_VERSION;

    const cJSON* pContentHash = cJSON_GetObjectItemCaseSensitive(pRoot, "contentHash");
    if (pContentHash)
    {
        if (!cJSON_IsString(pContentHash) || !isSceneAssetContentHash(pContentHash->valuestring))
        {
            cJSON_Delete(pRoot);
            return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "contentHash");
        }
        snprintf(pManifest->contentHash, sizeof(pManifest->contentHash), "%s", pContentHash->valuestring);
    }

    const cJSON* pDependencies = cJSON_GetObjectItemCaseSensitive(pRoot, "dependencies");
    if (pDependencies)
    {
        if (!cJSON_IsArray(pDependencies) || cJSON_GetArraySize(pDependencies) > (int)SCENE_ASSET_MAX_DEPENDENCIES)
        {
            cJSON_Delete(pRoot);
            return failSceneAsset(pError, SCENE_ASSET_ERROR_CAPACITY_EXCEEDED, "dependencies");
        }
        pManifest->dependencyCount = (uint32_t)cJSON_GetArraySize(pDependencies);
        for (uint32_t i = 0; i < pManifest->dependencyCount; ++i)
        {
            const cJSON* pDependency = cJSON_GetArrayItem(pDependencies, (int)i);
            if (!cJSON_IsString(pDependency) || !pDependency->valuestring || !isSceneAssetRelativePath(pDependency->valuestring) ||
                strlen(pDependency->valuestring) >= FS_MAX_PATH)
            {
                cJSON_Delete(pRoot);
                return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_PATH, "dependencies");
            }
            snprintf(pManifest->dependencies[i], FS_MAX_PATH, "%s", pDependency->valuestring);
        }
    }

    if (!copySceneAssetString(pRoot, "defaultScene", pManifest->defaultScene, sizeof(pManifest->defaultScene), true, pError))
    {
        cJSON_Delete(pRoot);
        return false;
    }

    const cJSON* pScenes = cJSON_GetObjectItemCaseSensitive(pRoot, "scenes");
    const cJSON* pScene = cJSON_IsObject(pScenes) ? cJSON_GetObjectItemCaseSensitive(pScenes, pManifest->defaultScene) : nullptr;
    if (!cJSON_IsObject(pScene))
    {
        cJSON_Delete(pRoot);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_MISSING_FIELD, "default scene entry");
    }

    if (!copySceneAssetPath(pScene, "geometry", pManifest->geometry, true, pError) ||
        !copySceneAssetPath(pScene, "sourceGltf", pManifest->sourceGltf, false, pError) ||
        !copySceneAssetPath(pScene, "environment", pManifest->environment, false, pError))
    {
        cJSON_Delete(pRoot);
        return false;
    }

    const cJSON* pTextureDirectories = cJSON_GetObjectItemCaseSensitive(pScene, "textureDirectories");
    if (pTextureDirectories)
    {
        if (!cJSON_IsArray(pTextureDirectories))
        {
            cJSON_Delete(pRoot);
            return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "textureDirectories");
        }
        const int textureDirectoryCount = cJSON_GetArraySize(pTextureDirectories);
        if (textureDirectoryCount < 0 || textureDirectoryCount > (int)SCENE_ASSET_MAX_TEXTURE_DIRECTORIES)
        {
            cJSON_Delete(pRoot);
            return failSceneAsset(pError, SCENE_ASSET_ERROR_CAPACITY_EXCEEDED, "textureDirectories");
        }
        for (int i = 0; i < textureDirectoryCount; ++i)
        {
            const cJSON* pDirectory = cJSON_GetArrayItem(pTextureDirectories, i);
            if (!cJSON_IsString(pDirectory) || !pDirectory->valuestring || !isSceneAssetRelativePath(pDirectory->valuestring) ||
                strlen(pDirectory->valuestring) >= FS_MAX_PATH)
            {
                cJSON_Delete(pRoot);
                return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_PATH, "textureDirectories");
            }
            snprintf(pManifest->textureDirectories[i], FS_MAX_PATH, "%s", pDirectory->valuestring);
        }
        pManifest->textureDirectoryCount = (uint32_t)textureDirectoryCount;
    }

    const cJSON* pTextures = cJSON_GetObjectItemCaseSensitive(pScene, "textures");
    if (pTextures)
    {
        if (!cJSON_IsArray(pTextures) || cJSON_GetArraySize(pTextures) > (int)SCENE_ASSET_MAX_TEXTURES)
        {
            cJSON_Delete(pRoot);
            return failSceneAsset(pError, SCENE_ASSET_ERROR_CAPACITY_EXCEEDED, "textures");
        }
        pManifest->textureCount = (uint32_t)cJSON_GetArraySize(pTextures);
        for (uint32_t i = 0; i < pManifest->textureCount; ++i)
        {
            const cJSON* pTexture = cJSON_GetArrayItem(pTextures, (int)i);
            if (!cJSON_IsObject(pTexture) || !copySceneAssetPath(pTexture, "path", pManifest->textures[i].path, true, pError))
            {
                cJSON_Delete(pRoot);
                return false;
            }
            if (!isSceneAssetTexturePath(pManifest->textures[i].path))
            {
                cJSON_Delete(pRoot);
                return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "textures.path must be DDS or KTX");
            }
            const cJSON* pSrgb = cJSON_GetObjectItemCaseSensitive(pTexture, "srgb");
            if (pSrgb && !cJSON_IsBool(pSrgb))
            {
                cJSON_Delete(pRoot);
                return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "textures.srgb");
            }
            pManifest->textures[i].srgb = cJSON_IsTrue(pSrgb);
            const cJSON* pCooked = cJSON_GetObjectItemCaseSensitive(pTexture, "cooked");
            if (pCooked && !cJSON_IsBool(pCooked))
            {
                cJSON_Delete(pRoot);
                return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "textures.cooked");
            }
            pManifest->textures[i].cooked = cJSON_IsTrue(pCooked);
        }
    }

    const cJSON* pMaterials = cJSON_GetObjectItemCaseSensitive(pScene, "materials");
    if (pMaterials)
    {
        if (!cJSON_IsArray(pMaterials) || cJSON_GetArraySize(pMaterials) > (int)SCENE_ASSET_MAX_MATERIALS)
        {
            cJSON_Delete(pRoot);
            return failSceneAsset(pError, SCENE_ASSET_ERROR_CAPACITY_EXCEEDED, "materials");
        }
        pManifest->materialCount = (uint32_t)cJSON_GetArraySize(pMaterials);
        for (uint32_t i = 0; i < pManifest->materialCount; ++i)
        {
            const cJSON*                pMaterial = cJSON_GetArrayItem(pMaterials, (int)i);
            SceneAssetMaterialManifest* pOutput = &pManifest->materials[i];
            pOutput->baseColorTexture = -1;
            pOutput->normalTexture = -1;
            pOutput->metallicRoughnessTexture = -1;
            pOutput->emissiveTexture = -1;
            pOutput->baseColorFactor[0] = 1.0f;
            pOutput->baseColorFactor[1] = 1.0f;
            pOutput->baseColorFactor[2] = 1.0f;
            pOutput->baseColorFactor[3] = 1.0f;
            pOutput->metallicFactor = 1.0f;
            pOutput->roughnessFactor = 1.0f;
            if (!cJSON_IsObject(pMaterial) ||
                !copySceneAssetString(pMaterial, "name", pOutput->name, sizeof(pOutput->name), false, pError) ||
                !copySceneAssetTextureIndex(pMaterial, "baseColorTexture", pManifest->textureCount, &pOutput->baseColorTexture, pError) ||
                !copySceneAssetTextureIndex(pMaterial, "normalTexture", pManifest->textureCount, &pOutput->normalTexture, pError) ||
                !copySceneAssetTextureIndex(pMaterial, "metallicRoughnessTexture", pManifest->textureCount,
                                            &pOutput->metallicRoughnessTexture, pError) ||
                !copySceneAssetTextureIndex(pMaterial, "emissiveTexture", pManifest->textureCount, &pOutput->emissiveTexture, pError) ||
                !copySceneAssetFloatArray(pMaterial, "baseColorFactor", pOutput->baseColorFactor, 4, pError) ||
                !copySceneAssetFloatArray(pMaterial, "emissiveFactor", pOutput->emissiveFactor, 3, pError))
            {
                cJSON_Delete(pRoot);
                return false;
            }
            const cJSON* pMetallic = cJSON_GetObjectItemCaseSensitive(pMaterial, "metallicFactor");
            const cJSON* pRoughness = cJSON_GetObjectItemCaseSensitive(pMaterial, "roughnessFactor");
            if ((pMetallic && !cJSON_IsNumber(pMetallic)) || (pRoughness && !cJSON_IsNumber(pRoughness)))
            {
                cJSON_Delete(pRoot);
                return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "material factor");
            }
            if (pMetallic)
                pOutput->metallicFactor = (float)pMetallic->valuedouble;
            if (pRoughness)
                pOutput->roughnessFactor = (float)pRoughness->valuedouble;
        }
    }

    const cJSON* pConvention = cJSON_GetObjectItemCaseSensitive(pScene, "materialConvention");
    if (pConvention && !cJSON_IsObject(pConvention))
    {
        cJSON_Delete(pRoot);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_FIELD, "materialConvention");
    }
    if (pConvention && (!copySceneAssetString(pConvention, "baseColor", pManifest->materialConvention.baseColor,
                                              sizeof(pManifest->materialConvention.baseColor), false, pError) ||
                        !copySceneAssetString(pConvention, "specular", pManifest->materialConvention.specular,
                                              sizeof(pManifest->materialConvention.specular), false, pError) ||
                        !copySceneAssetString(pConvention, "normal", pManifest->materialConvention.normal,
                                              sizeof(pManifest->materialConvention.normal), false, pError) ||
                        !copySceneAssetString(pConvention, "emissive", pManifest->materialConvention.emissive,
                                              sizeof(pManifest->materialConvention.emissive), false, pError)))
    {
        cJSON_Delete(pRoot);
        return false;
    }

    cJSON_Delete(pRoot);
    return true;
}

bool loadSceneAssetManifest(ResourceDirectory resourceDirectory, const char* pFileName, SceneAssetManifest* pManifest,
                            SceneAssetError* pError)
{
    if (pError)
        *pError = {};
    if (!pFileName || !pFileName[0] || !pManifest)
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_ARGUMENT, "manifest file");

    FileStream stream = {};
    if (!fsOpenStreamFromPath(resourceDirectory, pFileName, FM_READ, &stream))
        return failSceneAsset(pError, SCENE_ASSET_ERROR_IO, "could not open manifest");

    const ssize_t fileSize = fsGetStreamFileSize(&stream);
    if (fileSize <= 0 || (size_t)fileSize > kMaxSceneAssetManifestSize)
    {
        fsCloseStream(&stream);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_IO, "invalid manifest file size");
    }

    char* pJson = (char*)tf_malloc((size_t)fileSize);
    if (!pJson)
    {
        fsCloseStream(&stream);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_IO, "manifest allocation failed");
    }

    const size_t bytesRead = fsReadFromStream(&stream, pJson, (size_t)fileSize);
    fsCloseStream(&stream);
    if (bytesRead != (size_t)fileSize)
    {
        tf_free(pJson);
        return failSceneAsset(pError, SCENE_ASSET_ERROR_IO, "could not read manifest");
    }

    const bool parsed = parseSceneAssetManifest(pJson, bytesRead, pManifest, pError);
    tf_free(pJson);
    return parsed;
}

bool prepareSceneAssetGeometryLoadDesc(const SceneAssetManifest* pManifest, const GeometryLoadDesc* pSource,
                                       GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError)
{
    if (pError)
        *pError = {};
    if (!pManifest || !pSource || !pGeometryLoadDesc || !pManifest->geometry[0] || !pSource->pVertexLayout ||
        (!pSource->ppGeometry && !pSource->ppGeometryData))
        return failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_ARGUMENT, "geometry load descriptor");

    *pGeometryLoadDesc = *pSource;
    pGeometryLoadDesc->pFileName = pManifest->geometry;
    return true;
}

namespace hz
{
// Only the scene implementation can adopt loader outputs. Call after token completion.
struct SceneResourceAccess
{
    static GPUBuffer take(RenderContext* pContext, Buffer*& pBuffer, ResourceState state)
    {
        GPUBuffer result(pContext, pBuffer, pBuffer->size, (ResourceMemoryUsage)pBuffer->memoryUsage, (DescriptorType)pBuffer->descriptors,
                         state);
        pBuffer = nullptr;
        return result;
    }

    static GPUTexture take(RenderContext* pContext, Texture*& pTexture)
    {
        GPUTexture result(pContext, pTexture, nullptr, RESOURCE_STATE_SHADER_RESOURCE);
        pTexture = nullptr;
        return result;
    }
};
} // namespace hz

struct SceneGpuResources
{
    SceneGeometry    geometry;
    hz::GPUBuffer    materials;
    hz::GPUTexture** ppTextures = nullptr;
};

struct SceneAssetSlot
{
    SceneAssetManifest*    pManifest;
    Geometry*              pGeometry;
    Texture**              ppTextures;
    SceneAssetGpuMaterial* pGpuMaterials;
    Buffer*                pMaterialBuffer;
    SceneGpuResources*     pGpuResources;
    SyncToken              geometryToken;
    SyncToken*             pTextureTokens;
    SyncToken              materialToken;
    bool*                  pTextureResident;
    uint32_t               generation;
    SceneAssetStatus       status;
    bool                   occupied;
    bool                   retiring;
    bool                   geometryResident;
    bool                   materialResident;
};

static void defaultLoadSceneGeometry(GeometryLoadDesc* pDesc, SyncToken* pToken, void*) { addResource(pDesc, pToken); }
static void defaultLoadSceneTexture(TextureLoadDesc* pDesc, SyncToken* pToken, void*) { addResource(pDesc, pToken); }
static void defaultLoadSceneBuffer(BufferLoadDesc* pDesc, SyncToken* pToken, void*) { addResource(pDesc, pToken); }
static bool defaultIsSceneTokenCompleted(const SyncToken* pToken, void*) { return isTokenCompleted(pToken); }
static void defaultWaitForSceneToken(const SyncToken* pToken, void*) { waitForToken(pToken); }
static void defaultRemoveSceneGeometry(void*, void* pResource) { removeResource((Geometry*)pResource); }
static void defaultRemoveSceneTexture(void*, void* pResource) { removeResource((Texture*)pResource); }
static void defaultRemoveSceneBuffer(void*, void* pResource) { removeResource((Buffer*)pResource); }

static SceneAssetHandle invalidSceneAssetHandle() { return { UINT32_MAX, 0 }; }

SceneAssetSlot* SceneManager::findSlot(SceneAssetHandle handle)
{
    if (handle.index >= arrlenu(pSlots))
        return nullptr;
    SceneAssetSlot* pSlot = &pSlots[handle.index];
    return pSlot->occupied && !pSlot->retiring && pSlot->generation == handle.generation ? pSlot : nullptr;
}

const SceneAssetSlot* SceneManager::findSlot(SceneAssetHandle handle) const { return const_cast<SceneManager*>(this)->findSlot(handle); }

void SceneManager::destroySlot(SceneAssetSlot* pSlot)
{
    if (pSlot->pGpuResources)
    {
        for (uint32_t i = 0; i < arrlenu(pSlot->pGpuResources->ppTextures); ++i)
        {
            if (pSlot->pGpuResources->ppTextures[i])
                tf_delete(pSlot->pGpuResources->ppTextures[i]);
        }
        arrfree(pSlot->pGpuResources->ppTextures);
        tf_delete(pSlot->pGpuResources);
    }
    if (pSlot->pGeometry)
        callbacks.pRemoveGeometry(pUserData, pSlot->pGeometry);
    if (pSlot->pMaterialBuffer)
        callbacks.pRemoveBuffer(pUserData, pSlot->pMaterialBuffer);
    if (pSlot->pManifest)
    {
        for (uint32_t i = 0; i < pSlot->pManifest->textureCount; ++i)
        {
            if (pSlot->ppTextures && pSlot->ppTextures[i])
                callbacks.pRemoveTexture(pUserData, pSlot->ppTextures[i]);
        }
    }
    arrfree(pSlot->ppTextures);
    arrfree(pSlot->pTextureTokens);
    arrfree(pSlot->pTextureResident);
    arrfree(pSlot->pGpuMaterials);
    tf_free(pSlot->pManifest);

    const uint32_t generation = pSlot->generation;
    *pSlot = {};
    pSlot->generation = generation;
}

SceneManager::SceneManager(const SceneManagerDesc& desc)
{
    ASSERT(desc.capacity);
    ASSERT(desc.pContext);

    SceneAssetResourceCallbacks callbacks = desc.callbacks;
    const bool                  hasCustomCallbacks = callbacks.pLoadGeometry || callbacks.pLoadTexture || callbacks.pLoadBuffer ||
                                    callbacks.pIsTokenCompleted || callbacks.pWaitForToken || callbacks.pRemoveGeometry ||
                                    callbacks.pRemoveTexture || callbacks.pRemoveBuffer;
    if (!hasCustomCallbacks)
    {
        callbacks = {
            .pLoadGeometry = defaultLoadSceneGeometry,
            .pLoadTexture = defaultLoadSceneTexture,
            .pLoadBuffer = defaultLoadSceneBuffer,
            .pIsTokenCompleted = defaultIsSceneTokenCompleted,
            .pWaitForToken = defaultWaitForSceneToken,
            .pRemoveGeometry = defaultRemoveSceneGeometry,
            .pRemoveTexture = defaultRemoveSceneTexture,
            .pRemoveBuffer = defaultRemoveSceneBuffer,
        };
    }
    ASSERT(callbacks.pLoadGeometry && callbacks.pLoadTexture && callbacks.pLoadBuffer && callbacks.pIsTokenCompleted &&
           callbacks.pWaitForToken && callbacks.pRemoveGeometry && callbacks.pRemoveTexture && callbacks.pRemoveBuffer);

    // Resource loader callbacks retain addresses inside the slots. Size once and never relocate them.
    arrsetlen(pSlots, desc.capacity);
    for (uint32_t i = 0; i < desc.capacity; ++i)
        pSlots[i] = {};
    this->callbacks = callbacks;
    pContext = desc.pContext;
    pUserData = desc.pUserData;
    pEnsureGltfCooked = desc.pEnsureGltfCooked;
}

SceneManager::~SceneManager()
{
    for (uint32_t i = 0; i < arrlenu(pSlots); ++i)
    {
        SceneAssetSlot* pSlot = &pSlots[i];
        if (!pSlot->occupied)
            continue;
        if (pSlot->status == SCENE_ASSET_STATUS_LOADING)
        {
            callbacks.pWaitForToken(&pSlot->geometryToken, pUserData);
            for (uint32_t textureIndex = 0; textureIndex < pSlot->pManifest->textureCount; ++textureIndex)
                callbacks.pWaitForToken(&pSlot->pTextureTokens[textureIndex], pUserData);
            if (pSlot->pManifest->materialCount)
                callbacks.pWaitForToken(&pSlot->materialToken, pUserData);
        }
        destroySlot(pSlot);
    }
    arrfree(pSlots);
}

void SceneManager::update()
{
    for (uint32_t i = 0; i < arrlenu(pSlots); ++i)
    {
        SceneAssetSlot* pSlot = &pSlots[i];
        if (!pSlot->occupied || pSlot->status != SCENE_ASSET_STATUS_LOADING)
            continue;
        if (!pSlot->geometryResident && !pSlot->retiring && callbacks.pIsTokenCompleted(&pSlot->geometryToken, pUserData) &&
            pSlot->pGeometry)
        {
            Geometry* pSource = pSlot->pGeometry;
            bool      valid = !pSource->pGeometryBuffer && pSource->pIndexBuffer && pSource->vertexBufferCount <= MAX_VERTEX_BINDINGS;
            for (uint32_t binding = 0; valid && binding < pSource->vertexBufferCount; ++binding)
                valid = pSource->pVertexBuffers[binding] != nullptr;
            if (valid)
            {
                SceneGeometry& geometry = pSlot->pGpuResources->geometry;
                geometry.indexBuffer = hz::SceneResourceAccess::take(pContext, pSource->pIndexBuffer, gIndexBufferState);
                for (uint32_t binding = 0; binding < pSource->vertexBufferCount; ++binding)
                {
                    geometry.vertexBuffers[binding] =
                        hz::SceneResourceAccess::take(pContext, pSource->pVertexBuffers[binding], gVertexBufferState);
                }
                geometry.pDrawArgs = pSource->pDrawArgs;
                memcpy(geometry.vertexStrides, pSource->vertexStrides, sizeof(geometry.vertexStrides));
                geometry.vertexBufferCount = pSource->vertexBufferCount;
                geometry.indexType = (IndexType)pSource->indexType;
                geometry.drawArgCount = pSource->drawArgCount;
                geometry.indexCount = pSource->indexCount;
                geometry.vertexCount = pSource->vertexCount;
                pSlot->geometryResident = true;
            }
        }
        bool allCompleted = callbacks.pIsTokenCompleted(&pSlot->geometryToken, pUserData);
        bool loaded = pSlot->geometryResident;
        for (uint32_t textureIndex = 0; textureIndex < pSlot->pManifest->textureCount; ++textureIndex)
        {
            const bool completed = callbacks.pIsTokenCompleted(&pSlot->pTextureTokens[textureIndex], pUserData);
            if (!pSlot->pTextureResident[textureIndex] && !pSlot->retiring && completed && pSlot->ppTextures[textureIndex])
            {
                pSlot->pGpuResources->ppTextures[textureIndex] =
                    tf_new(hz::GPUTexture, hz::SceneResourceAccess::take(pContext, pSlot->ppTextures[textureIndex]));
                pSlot->pTextureResident[textureIndex] = true;
            }
            allCompleted = allCompleted && completed;
            loaded = loaded && pSlot->pTextureResident[textureIndex];
        }
        if (pSlot->pManifest->materialCount)
        {
            const bool completed = callbacks.pIsTokenCompleted(&pSlot->materialToken, pUserData);
            if (!pSlot->materialResident && !pSlot->retiring && completed && pSlot->pMaterialBuffer)
            {
                pSlot->pGpuResources->materials =
                    hz::SceneResourceAccess::take(pContext, pSlot->pMaterialBuffer, RESOURCE_STATE_SHADER_RESOURCE);
                pSlot->materialResident = true;
            }
            allCompleted = allCompleted && completed;
            loaded = loaded && pSlot->materialResident;
        }
        else
            pSlot->materialResident = true;
        if (!allCompleted)
            continue;
        if (pSlot->retiring)
            destroySlot(pSlot);
        else
            pSlot->status = loaded ? SCENE_ASSET_STATUS_READY : SCENE_ASSET_STATUS_FAILED;
    }
}

SceneAssetHandle SceneManager::requestFromManifest(const SceneAssetManifest* pManifest, const GeometryLoadDesc* pGeometryLoadDesc)
{
    if (!pManifest || !pGeometryLoadDesc || !pGeometryLoadDesc->pVertexLayout || !pManifest->geometry[0] ||
        pGeometryLoadDesc->pGeometryBuffer || pManifest->textureCount > SCENE_ASSET_MAX_TEXTURES ||
        pManifest->materialCount > SCENE_ASSET_MAX_MATERIALS)
        return invalidSceneAssetHandle();

    uint32_t slotIndex = UINT32_MAX;
    for (uint32_t i = 0; i < arrlenu(pSlots); ++i)
    {
        if (!pSlots[i].occupied)
        {
            slotIndex = i;
            break;
        }
    }
    if (slotIndex == UINT32_MAX)
        return invalidSceneAssetHandle();

    SceneAssetSlot* pSlot = &pSlots[slotIndex];
    if (!pSlot->generation)
        pSlot->generation = 1;
    pSlot->pManifest = (SceneAssetManifest*)tf_malloc(sizeof(SceneAssetManifest));
    if (!pSlot->pManifest)
        return invalidSceneAssetHandle();
    // stb_ds holds pointers only; non-trivial GPU owners have stable storage and real destructors.
    pSlot->pGpuResources = tf_new(SceneGpuResources);
    // Finish sizing before enqueueing loads, which write into these arrays asynchronously.
    if (pManifest->textureCount)
    {
        arrsetlen(pSlot->ppTextures, pManifest->textureCount);
        arrsetlen(pSlot->pTextureTokens, pManifest->textureCount);
        arrsetlen(pSlot->pTextureResident, pManifest->textureCount);
        arrsetlen(pSlot->pGpuResources->ppTextures, pManifest->textureCount);
        for (uint32_t i = 0; i < pManifest->textureCount; ++i)
        {
            pSlot->ppTextures[i] = nullptr;
            pSlot->pTextureTokens[i] = 0;
            pSlot->pTextureResident[i] = false;
            pSlot->pGpuResources->ppTextures[i] = nullptr;
        }
    }
    if (pManifest->materialCount)
        arrsetlen(pSlot->pGpuMaterials, pManifest->materialCount);
    *pSlot->pManifest = *pManifest;
    pSlot->occupied = true;
    pSlot->status = SCENE_ASSET_STATUS_LOADING;

    GeometryLoadDesc geometryLoad = *pGeometryLoadDesc;
    geometryLoad.ppGeometry = &pSlot->pGeometry;
    geometryLoad.pFileName = pSlot->pManifest->geometry;
    callbacks.pLoadGeometry(&geometryLoad, &pSlot->geometryToken, pUserData);

    for (uint32_t i = 0; i < pSlot->pManifest->textureCount; ++i)
    {
        TextureLoadDesc textureLoad = {
            .ppTexture = &pSlot->ppTextures[i],
            .pFileName = pSlot->pManifest->textures[i].path,
            .creationFlag = pSlot->pManifest->textures[i].srgb ? TEXTURE_CREATION_FLAG_SRGB : TEXTURE_CREATION_FLAG_NONE,
            .container = getSceneAssetTextureContainer(pSlot->pManifest->textures[i].path),
            .resourceDirectory = pSlot->pManifest->textures[i].cooked ? RD_MESHES : RD_TEXTURES,
        };
        callbacks.pLoadTexture(&textureLoad, &pSlot->pTextureTokens[i], pUserData);
    }

    for (uint32_t i = 0; i < pSlot->pManifest->materialCount; ++i)
    {
        const SceneAssetMaterialManifest* pSource = &pSlot->pManifest->materials[i];
        SceneAssetGpuMaterial*            pDestination = &pSlot->pGpuMaterials[i];
        *pDestination = {};
        pDestination->baseColorTexture = pSource->baseColorTexture >= 0 ? (uint32_t)pSource->baseColorTexture : UINT32_MAX;
        pDestination->normalTexture = pSource->normalTexture >= 0 ? (uint32_t)pSource->normalTexture : UINT32_MAX;
        pDestination->metallicRoughnessTexture =
            pSource->metallicRoughnessTexture >= 0 ? (uint32_t)pSource->metallicRoughnessTexture : UINT32_MAX;
        pDestination->emissiveTexture = pSource->emissiveTexture >= 0 ? (uint32_t)pSource->emissiveTexture : UINT32_MAX;
        memcpy(pDestination->baseColorFactor, pSource->baseColorFactor, sizeof(pDestination->baseColorFactor));
        memcpy(pDestination->emissiveFactor, pSource->emissiveFactor, sizeof(pDestination->emissiveFactor));
        pDestination->metallicFactor = pSource->metallicFactor;
        pDestination->roughnessFactor = pSource->roughnessFactor;
    }
    if (pSlot->pManifest->materialCount)
    {
        BufferLoadDesc materialBufferLoad = {
            .ppBuffer = &pSlot->pMaterialBuffer,
            .pData = pSlot->pGpuMaterials,
            .desc = {
                .size = sizeof(SceneAssetGpuMaterial) * pSlot->pManifest->materialCount,
                .elementCount = pSlot->pManifest->materialCount,
                .structStride = sizeof(SceneAssetGpuMaterial),
                .pName = "SceneAssetMaterialBuffer",
                .memoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                .startState = RESOURCE_STATE_SHADER_RESOURCE,
                .descriptors = DESCRIPTOR_TYPE_BUFFER,
            },
        };
        callbacks.pLoadBuffer(&materialBufferLoad, &pSlot->materialToken, pUserData);
    }
    return { slotIndex, pSlot->generation };
}

SceneAssetHandle SceneManager::requestFromManifestFile(ResourceDirectory resourceDirectory, const char* pManifestFileName,
                                                       const GeometryLoadDesc* pGeometryLoadDesc, SceneAssetError* pError)
{
    if (pError)
        *pError = {};
    SceneAssetManifest* pManifest = (SceneAssetManifest*)tf_malloc(sizeof(SceneAssetManifest));
    if (!pManifest)
    {
        failSceneAsset(pError, SCENE_ASSET_ERROR_IO, "manifest allocation failed");
        return invalidSceneAssetHandle();
    }
    if (!loadSceneAssetManifest(resourceDirectory, pManifestFileName, pManifest, pError))
    {
        tf_free(pManifest);
        return invalidSceneAssetHandle();
    }
    const SceneAssetHandle handle = requestFromManifest(pManifest, pGeometryLoadDesc);
    tf_free(pManifest);
    if (!isSceneAssetHandleValid(handle))
        failSceneAsset(pError, SCENE_ASSET_ERROR_CAPACITY_EXCEEDED, "scene asset capacity exhausted");
    return handle;
}

SceneAssetHandle SceneManager::requestFromGltf(ResourceDirectory sourceDirectory, const char* pSourceFile,
                                               ResourceDirectory outputDirectory, const GeometryLoadDesc* pGeometryLoadDesc,
                                               SceneAssetError* pError)
{
    if (pError)
        *pError = {};
    if (!pGeometryLoadDesc || !pGeometryLoadDesc->pVertexLayout || !isSceneAssetRelativePath(pSourceFile) ||
        strlen(pSourceFile) + sizeof(".scene.json") >= FS_MAX_PATH || !hasSceneAssetTextureExtension(pSourceFile, ".gltf"))
    {
        failSceneAsset(pError, SCENE_ASSET_ERROR_INVALID_ARGUMENT, "glTF scene request");
        return invalidSceneAssetHandle();
    }
    if (pEnsureGltfCooked && !pEnsureGltfCooked(sourceDirectory, pSourceFile, outputDirectory, pError))
    {
        if (pError && pError->code == SCENE_ASSET_ERROR_NONE)
            failSceneAsset(pError, SCENE_ASSET_ERROR_IO, "glTF cooking failed");
        return invalidSceneAssetHandle();
    }
    char manifest[FS_MAX_PATH] = {};
    fsReplacePathExtension(pSourceFile, "scene.json", manifest);
    return requestFromManifestFile(outputDirectory, manifest, pGeometryLoadDesc, pError);
}

bool SceneManager::release(SceneAssetHandle handle)
{
    SceneAssetSlot* pSlot = findSlot(handle);
    if (!pSlot)
        return false;
    ++pSlot->generation;
    if (!pSlot->generation)
        ++pSlot->generation;
    if (pSlot->status == SCENE_ASSET_STATUS_LOADING)
        pSlot->retiring = true;
    else
        destroySlot(pSlot);
    return true;
}

bool isSceneAssetHandleValid(SceneAssetHandle handle) { return handle.index != UINT32_MAX && handle.generation != 0; }

SceneAssetStatus SceneManager::getStatus(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot ? pSlot->status : SCENE_ASSET_STATUS_INVALID;
}

const SceneGeometry* SceneManager::getGeometry(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot && pSlot->geometryResident ? &pSlot->pGpuResources->geometry : nullptr;
}

uint32_t SceneManager::getTextureCount(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot ? pSlot->pManifest->textureCount : 0;
}

const hz::GPUTexture* SceneManager::getTexture(SceneAssetHandle handle, uint32_t textureIndex) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot && textureIndex < pSlot->pManifest->textureCount ? pSlot->pGpuResources->ppTextures[textureIndex] : nullptr;
}

const hz::GPUBuffer* SceneManager::getMaterialBuffer(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot && pSlot->materialResident && pSlot->pManifest->materialCount ? &pSlot->pGpuResources->materials : nullptr;
}

uint32_t SceneManager::getMaterialCount(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot ? pSlot->pManifest->materialCount : 0;
}

const SceneAssetGpuMaterial* SceneManager::getGpuMaterials(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot ? pSlot->pGpuMaterials : nullptr;
}

const SceneAssetManifest* SceneManager::getManifest(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot ? pSlot->pManifest : nullptr;
}

bool SceneManager::isGeometryResident(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot && pSlot->geometryResident;
}
bool SceneManager::isMaterialResident(SceneAssetHandle handle) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot && pSlot->materialResident;
}
bool SceneManager::isTextureResident(SceneAssetHandle handle, uint32_t textureIndex) const
{
    const SceneAssetSlot* pSlot = findSlot(handle);
    return pSlot && textureIndex < pSlot->pManifest->textureCount && pSlot->pTextureResident[textureIndex];
}
