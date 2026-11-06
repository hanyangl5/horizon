/* Copyright (c) 2026 Horizon */

#include "Scene/TextureAsset.h"
#include "SceneResourceAccess.h"
#include "Resources/IResourceLoader.h"
#include <string.h>

#include "Core/IMemory.h"

namespace hz
{
class TextureAssetData
{
public:
    explicit TextureAssetData(const AssetCatalogEntry& entry): entry(entry) {}
    ~TextureAssetData()
    {
        if (pUpload)
        {
            removeResource(pUpload);
        }
    }

    const AssetCatalogEntry& entry;
    GPUTexture               texture;
    Texture*                 pUpload = nullptr;
    SyncToken                token = 0;
    bool                     uploadSucceeded = false;
};

class TextureRequest
{
public:
    ~TextureRequest() { tf_delete(pAsset); }
    TextureAsset* pAsset = nullptr;
};

class TextureAssetLoaderData
{
public:
    TextureAssetLoaderData(RenderContext& context, ResourceDirectory cooked, ResourceDirectory source):
        context(context), cookedDirectory(cooked), sourceDirectory(source)
    {
    }

    static void* begin(const AssetCatalogEntry& entry, AssetHandle, void*)
    {
        if (entry.type != AssetType::Texture || entry.location.elementIndex != UINT32_MAX)
        {
            LOGF(eERROR, "Invalid TextureAsset location '%s'", entry.location.pPath);
            return nullptr;
        }
        TextureRequest* pRequest = tf_new(TextureRequest);
        pRequest->pAsset = tf_new(TextureAsset);
        pRequest->pAsset->pData = tf_new(TextureAssetData, entry);
        return pRequest;
    }

    static AssetLoadResult poll(void* pJob, void*)
    {
        TextureRequest* pRequest = (TextureRequest*)pJob;
        TextureAsset*   pAsset = pRequest->pAsset;
        pRequest->pAsset = nullptr;
        return { .status = AssetLoadStatus::Succeeded, .pPayload = pAsset };
    }
    static void destroyRequest(void* pJob, void*) { tf_delete((TextureRequest*)pJob); }
    static void destroyPayload(void* pPayload, void*) { tf_delete((TextureAsset*)pPayload); }

    static void upload(void* pPayload, const AssetRegistry&, Span<AssetHandle>, void* pUser)
    {
        TextureAssetLoaderData& loader = *(TextureAssetLoaderData*)pUser;
        TextureAssetData&       data = *((TextureAsset*)pPayload)->pData;
        const char*             pExtension = strrchr(data.entry.location.pPath, '.');
        if (!pExtension || (stricmp(pExtension, ".dds") && stricmp(pExtension, ".ktx")))
        {
            LOGF(eERROR, "TextureAsset requires DDS or KTX: '%s'", data.entry.location.pPath);
            return;
        }
        TextureLoadDesc desc = {
            .ppTexture = &data.pUpload,
            .pFileName = data.entry.location.pPath,
            .creationFlag = (TextureCreationFlags)(TEXTURE_CREATION_FLAG_FORCE_2D | (data.entry.srgb ? TEXTURE_CREATION_FLAG_SRGB : 0)),
            .container = stricmp(pExtension, ".ktx") ? TEXTURE_CONTAINER_DDS : TEXTURE_CONTAINER_KTX,
            .resourceDirectory = data.entry.location.root == AssetPathRoot::Cooked ? loader.cookedDirectory : loader.sourceDirectory,
            .pSuccess = &data.uploadSucceeded,
        };
        addResource(&desc, &data.token);
    }

    static AssetLoadStatus pollUpload(void* pPayload, void* pUser)
    {
        TextureAssetData& data = *((TextureAsset*)pPayload)->pData;
        if (!isTokenCompleted(&data.token))
        {
            return AssetLoadStatus::Pending;
        }
        if (!data.uploadSucceeded || !data.pUpload)
        {
            LOGF(eERROR, "TextureAsset upload failed: '%s'", data.entry.location.pPath);
            return AssetLoadStatus::Failed;
        }
        data.texture = SceneResourceAccess::take(&((TextureAssetLoaderData*)pUser)->context, data.pUpload);
        return AssetLoadStatus::Succeeded;
    }
    static void waitUpload(void* pPayload, void*) { waitForToken(&((TextureAsset*)pPayload)->pData->token); }

    RenderContext&    context;
    ResourceDirectory cookedDirectory;
    ResourceDirectory sourceDirectory;
};

TextureAsset::~TextureAsset() { tf_delete(pData); }
const GPUTexture& TextureAsset::getTexture() const
{
    ASSERT(pData);
    return pData->texture;
}

TextureAssetLoader::TextureAssetLoader(RenderContext& context, ResourceDirectory cooked, ResourceDirectory source):
    pData(tf_new(TextureAssetLoaderData, context, cooked, source))
{
}
TextureAssetLoader::~TextureAssetLoader() { tf_delete(pData); }
AssetLoader TextureAssetLoader::getLoader()
{
    return { .pUserData = pData,
             .pBegin = TextureAssetLoaderData::begin,
             .pPoll = TextureAssetLoaderData::poll,
             .pDestroyRequest = TextureAssetLoaderData::destroyRequest,
             .pDestroyPayload = TextureAssetLoaderData::destroyPayload,
             .pUpload = TextureAssetLoaderData::upload,
             .pPollUpload = TextureAssetLoaderData::pollUpload,
             .pWaitUpload = TextureAssetLoaderData::waitUpload };
}
} // namespace hz
