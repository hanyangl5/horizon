/* Copyright (c) 2026 Horizon */

#include "Scene/SceneAssetLoader.h"
#include "Scene/SceneAsset.h"
#include "Core/IContainer.h"
#include "Core/IThread.h"
#include <stdlib.h>

#include "Core/IMemory.h"

namespace hz
{
class SceneLoadRequest
{
public:
    SceneLoadRequest(const AssetCatalogEntry& entry, ResourceDirectory directory): entry(entry), directory(directory) {}
    ~SceneLoadRequest() { MemDelete(pScene); }

    bool matchesDependencies() const
    {
        Array<AssetID> dependencies;
        dependencies.Reserve(pScene->getNodes().Count);
        for (const SceneAssetNode& node : pScene->getNodes())
            if (node.mesh.IsValid())
                dependencies.PushBack(node.mesh);
        if (!dependencies.Empty())
            qsort(dependencies.GetData(), dependencies.Size(), sizeof(AssetID), compareIDs);
        uint32_t Count = 0;
        for (uint32_t i = 0; i < dependencies.Size(); ++i)
        {
            if (i && dependencies[i] == dependencies[i - 1])
                continue;
            if (Count == entry.dependencies.Count || dependencies[i] != entry.dependencies.Data[Count++])
                return false;
        }
        return Count == entry.dependencies.Count;
    }

    bool decode()
    {
        FileStream stream = {};
        if (!OpenStreamFromPath(directory, entry.location.pPath, FileModeRead, &stream))
        {
            LOGF(Error, "Cannot open SceneAsset '%s'", entry.location.pPath);
            return false;
        }
        const ssize_t Size = GetStreamFileSize(&stream);
        if (Size <= 0 || Size > 64 * 1024 * 1024)
        {
            CloseStream(&stream);
            LOGF(Error, "Invalid SceneAsset file size for '%s'", entry.location.pPath);
            return false;
        }
        Array<char>  json((uint32_t)Size);
        const size_t bytesRead = ReadFromStream(&stream, json.GetData(), (size_t)Size);
        CloseStream(&stream);
        if (bytesRead != (size_t)Size)
        {
            LOGF(Error, "Cannot read SceneAsset '%s'", entry.location.pPath);
            return false;
        }
        pScene = MemNew(SceneAsset);
        if (!pScene->Parse({ json.GetData(), json.Size() }) || pScene->getID() != entry.id)
        {
            LOGF(Error, "Invalid SceneAsset payload or mismatched identity in '%s'", entry.location.pPath);
            return false;
        }
        if (!matchesDependencies())
        {
            LOGF(Error, "SceneAsset dependencies differ from Catalog in '%s'", entry.location.pPath);
            return false;
        }
        return true;
    }

    static void execute(void* User, uint64_t)
    {
        SceneLoadRequest*     pRequest = (SceneLoadRequest*)User;
        const AssetLoadStatus status = pRequest->decode() ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed;
        tfrg_atomic32_store_release(&pRequest->status, (uint32_t)status);
    }

    const AssetCatalogEntry& entry;
    ResourceDirectory        directory;
    SceneAsset*              pScene = nullptr;
    tfrg_atomic32_t          status = (uint32_t)AssetLoadStatus::Pending;

private:
    static int compareIDs(const void* pA, const void* pB)
    {
        const AssetID& a = *(const AssetID*)pA;
        const AssetID& b = *(const AssetID*)pB;
        return (b < a) - (a < b);
    }
};

class SceneAssetLoaderData
{
public:
    explicit SceneAssetLoaderData(ResourceDirectory directory): directory(directory)
    {
        const ThreadSystemInitDesc desc = { .threadCount = 1, .threadName = "SceneAsset" };
        if (!threadSystemInit(&threads, &desc))
            LOGF(Error, "Cannot start SceneAsset loader worker");
    }
    ~SceneAssetLoaderData()
    {
        if (threads)
            threadSystemExit(&threads, &gThreadSystemExitDescDefault);
    }

    static void* begin(const AssetCatalogEntry& entry, AssetHandle, void* User)
    {
        SceneAssetLoaderData* pLoader = (SceneAssetLoaderData*)User;
        if (!pLoader->threads || entry.Type != AssetType::Scene || entry.location.root != AssetPathRoot::Cooked ||
            entry.location.elementIndex != UINT32_MAX)
        {
            LOGF(Error, "Cannot schedule SceneAsset '%s'", entry.location.pPath);
            return nullptr;
        }
        SceneLoadRequest* pRequest = MemNew(SceneLoadRequest, entry, pLoader->directory);
        threadSystemAddTask(pLoader->threads, SceneLoadRequest::execute, pRequest);
        return pRequest;
    }

    static AssetLoadResult poll(void* pJob, void*)
    {
        SceneLoadRequest*     pRequest = (SceneLoadRequest*)pJob;
        const AssetLoadStatus status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&pRequest->status);
        if (status == AssetLoadStatus::Pending)
            return {};
        SceneAsset* pScene = pRequest->pScene;
        pRequest->pScene = nullptr;
        return { .status = status, .pPayload = pScene };
    }

    static void destroyRequest(void* pJob, void* User)
    {
        SceneLoadRequest* pRequest = (SceneLoadRequest*)pJob;
        if ((AssetLoadStatus)tfrg_atomic32_load_acquire(&pRequest->status) == AssetLoadStatus::Pending)
            threadSystemWaitIdle(((SceneAssetLoaderData*)User)->threads);
        MemDelete(pRequest);
    }

    static void destroyPayload(void* pPayload, void*) { MemDelete((SceneAsset*)pPayload); }

    ResourceDirectory directory;
    ThreadSystem      threads = nullptr;
};

SceneAssetLoader::SceneAssetLoader(ResourceDirectory cookedDirectory): Data(MemNew(SceneAssetLoaderData, cookedDirectory)) {}
SceneAssetLoader::~SceneAssetLoader() { MemDelete(Data); }

AssetLoader SceneAssetLoader::getLoader()
{
    return { .UserData = Data,
             .pBegin = SceneAssetLoaderData::begin,
             .pPoll = SceneAssetLoaderData::poll,
             .pDestroyRequest = SceneAssetLoaderData::destroyRequest,
             .pDestroyPayload = SceneAssetLoaderData::destroyPayload };
}
} // namespace hz
