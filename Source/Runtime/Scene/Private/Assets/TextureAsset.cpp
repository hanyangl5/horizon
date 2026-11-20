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
    explicit TextureAssetData(const AssetCatalogEntry& Entry): entry(Entry) {}
    ~TextureAssetData()
    {
        if (pUpload)
            removeResource(pUpload);
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
    ~TextureRequest() { MemDelete(pAsset); }
    TextureAsset* pAsset = nullptr;
};

class TextureAssetLoaderData
{
public:
    TextureAssetLoaderData(RenderContext& Context, ResourceDirectory CookedValue, ResourceDirectory Source):
        context(Context), cookedDirectory(CookedValue), sourceDirectory(Source)
    {
    }

    static void* begin(const AssetCatalogEntry& Entry, AssetHandle, void*)
    {
        if (Entry.Type != AssetType::Texture || Entry.location.elementIndex != UINT32_MAX)
        {
            LOGF(Error, "Invalid TextureAsset location '%s'", Entry.location.pPath);
            return nullptr;
        }
        TextureRequest* Request = MemNew(TextureRequest);
        Request->pAsset = MemNew(TextureAsset);
        Request->pAsset->Data = MemNew(TextureAssetData, Entry);
        return Request;
    }

    static AssetLoadResult poll(void* Job, void*)
    {
        TextureRequest* Request = (TextureRequest*)Job;
        TextureAsset*   Asset = Request->pAsset;
        Request->pAsset = nullptr;
        return { .status = AssetLoadStatus::Succeeded, .pPayload = Asset };
    }
    static void destroyRequest(void* Job, void*) { MemDelete((TextureRequest*)Job); }
    static void destroyPayload(void* Payload, void*) { MemDelete((TextureAsset*)Payload); }

    static void Upload(void* Payload, const AssetManager&, Span<AssetHandle>, void* User)
    {
        TextureAssetLoaderData& Loader = *(TextureAssetLoaderData*)User;
        TextureAssetData&       DataValue = *((TextureAsset*)Payload)->Data;
        const char*             Extension = strrchr(DataValue.entry.location.pPath, '.');
        if (!Extension || (stricmp(Extension, ".dds") && stricmp(Extension, ".ktx")))
        {
            LOGF(Error, "TextureAsset requires DDS or KTX: '%s'", DataValue.entry.location.pPath);
            return;
        }
        TextureLoadDesc Desc = {
            .ppTexture = &DataValue.pUpload,
            .pFileName = DataValue.entry.location.pPath,
            .creationFlag = (TextureCreationFlags)(TEXTURE_CREATION_FLAG_FORCE_2D | (DataValue.entry.srgb ? TEXTURE_CREATION_FLAG_SRGB : 0)),
            .container = stricmp(Extension, ".ktx") ? TEXTURE_CONTAINER_DDS : TEXTURE_CONTAINER_KTX,
            .resourceDirectory = DataValue.entry.location.root == AssetPathRoot::Cooked ? Loader.cookedDirectory : Loader.sourceDirectory,
            .pSuccess = &DataValue.uploadSucceeded,
        };
        addResource(&Desc, &DataValue.token);
    }

    static AssetLoadStatus pollUpload(void* Payload, void* User)
    {
        TextureAssetData& DataValue = *((TextureAsset*)Payload)->Data;
        if (!isTokenCompleted(&DataValue.token))
            return AssetLoadStatus::Pending;
        if (!DataValue.uploadSucceeded || !DataValue.pUpload)
        {
            LOGF(Error, "TextureAsset Upload failed: '%s'", DataValue.entry.location.pPath);
            return AssetLoadStatus::Failed;
        }
        DataValue.texture = SceneResourceAccess::take(&((TextureAssetLoaderData*)User)->context, DataValue.pUpload);
        return AssetLoadStatus::Succeeded;
    }
    static void waitUpload(void* Payload, void*) { waitForToken(&((TextureAsset*)Payload)->Data->token); }

    RenderContext&    context;
    ResourceDirectory cookedDirectory;
    ResourceDirectory sourceDirectory;
};

TextureAsset::~TextureAsset() { MemDelete(Data); }
const GPUTexture& TextureAsset::GetTexture() const
{
    ASSERT(Data);
    return Data->texture;
}

TextureAssetLoader::TextureAssetLoader(RenderContext& Context, ResourceDirectory CookedValue, ResourceDirectory Source):
    Data(MemNew(TextureAssetLoaderData, Context, CookedValue, Source))
{
}
TextureAssetLoader::~TextureAssetLoader() { MemDelete(Data); }
AssetLoader TextureAssetLoader::GetLoader()
{
    return { .UserData = Data,
             .pBegin = TextureAssetLoaderData::begin,
             .pPoll = TextureAssetLoaderData::poll,
             .pDestroyRequest = TextureAssetLoaderData::destroyRequest,
             .pDestroyPayload = TextureAssetLoaderData::destroyPayload,
             .pUpload = TextureAssetLoaderData::Upload,
             .pPollUpload = TextureAssetLoaderData::pollUpload,
             .pWaitUpload = TextureAssetLoaderData::waitUpload };
}
} // namespace hz
