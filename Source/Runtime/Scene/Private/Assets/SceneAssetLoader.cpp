/* Copyright (c) 2026 Horizon */

#include "Scene/SceneAssetLoader.h"
#include "Scene/SceneAsset.h"
#include "AssetDependencies.h"
#include "Core/IContainer.h"
#include "Core/IThread.h"

#include "Core/IMemory.h"

namespace hz
{
class SceneLoadRequest
{
public:
    SceneLoadRequest(const AssetCatalogEntry& Entry, ResourceDirectory Directory): entry(Entry), directory(Directory) {}
    ~SceneLoadRequest() { MemDelete(pScene); }

    bool matchesDependencies() const
    {
        Array<AssetID> DependenciesValue;
        DependenciesValue.Reserve(pScene->GetNodes().Count * 3);
        for (const SceneAssetNode& NodeValue : pScene->GetNodes())
        {
            DependenciesValue.PushBack(NodeValue.mesh);
            DependenciesValue.PushBack(NodeValue.skeleton);
            DependenciesValue.PushBack(NodeValue.animation);
        }
        return MatchesAssetDependencies(entry, DependenciesValue);
    }

    bool decode()
    {
        FileStream Stream = {};
        if (!OpenStreamFromPath(directory, entry.location.pPath, FileModeRead, &Stream))
        {
            LOGF(Error, "Cannot open SceneAsset '%s'", entry.location.pPath);
            return false;
        }
        const ssize_t Size = GetStreamFileSize(&Stream);
        if (Size <= 0 || Size > 64 * 1024 * 1024)
        {
            CloseStream(&Stream);
            LOGF(Error, "Invalid SceneAsset file size for '%s'", entry.location.pPath);
            return false;
        }
        Array<char>  Json((uint32_t)Size);
        const size_t BytesRead = ReadFromStream(&Stream, Json.GetData(), (size_t)Size);
        CloseStream(&Stream);
        if (BytesRead != (size_t)Size)
        {
            LOGF(Error, "Cannot read SceneAsset '%s'", entry.location.pPath);
            return false;
        }
        pScene = MemNew(SceneAsset);
        if (!pScene->Parse({ Json.GetData(), Json.Size() }) || pScene->GetID() != entry.id)
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
        SceneLoadRequest*     Request = (SceneLoadRequest*)User;
        const AssetLoadStatus Status = Request->decode() ? AssetLoadStatus::Succeeded : AssetLoadStatus::Failed;
        tfrg_atomic32_store_release(&Request->status, (uint32_t)Status);
    }

    const AssetCatalogEntry& entry;
    ResourceDirectory        directory;
    SceneAsset*              pScene = nullptr;
    tfrg_atomic32_t          status = (uint32_t)AssetLoadStatus::Pending;
};

class SceneAssetLoaderData
{
public:
    explicit SceneAssetLoaderData(ResourceDirectory Directory): directory(Directory)
    {
        const ThreadSystemInitDesc Desc = { .threadCount = 1, .threadName = "SceneAsset" };
        if (!threadSystemInit(&threads, &Desc))
        {
            LOGF(Error, "Cannot start SceneAsset loader worker");
        }
    }
    ~SceneAssetLoaderData()
    {
        if (threads)
            threadSystemExit(&threads, &gThreadSystemExitDescDefault);
    }

    static void* begin(const AssetCatalogEntry& Entry, AssetHandle, void* User)
    {
        SceneAssetLoaderData* Loader = (SceneAssetLoaderData*)User;
        if (!Loader->threads || Entry.Type != AssetType::Scene || Entry.location.root != AssetPathRoot::Cooked ||
            Entry.location.elementIndex != UINT32_MAX)
        {
            LOGF(Error, "Cannot schedule SceneAsset '%s'", Entry.location.pPath);
            return nullptr;
        }
        SceneLoadRequest* Request = MemNew(SceneLoadRequest, Entry, Loader->directory);
        threadSystemAddTask(Loader->threads, SceneLoadRequest::execute, Request);
        return Request;
    }

    static AssetLoadResult poll(void* Job, void*)
    {
        SceneLoadRequest*     Request = (SceneLoadRequest*)Job;
        const AssetLoadStatus Status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&Request->status);
        if (Status == AssetLoadStatus::Pending)
        {
            return {};
        }
        SceneAsset* ScenePointer = Request->pScene;
        Request->pScene = nullptr;
        return { .status = Status, .pPayload = ScenePointer };
    }

    static void destroyRequest(void* Job, void* User)
    {
        SceneLoadRequest* Request = (SceneLoadRequest*)Job;
        if ((AssetLoadStatus)tfrg_atomic32_load_acquire(&Request->status) == AssetLoadStatus::Pending)
        {
            threadSystemWaitIdle(((SceneAssetLoaderData*)User)->threads);
        }
        MemDelete(Request);
    }

    static void destroyPayload(void* Payload, void*) { MemDelete((SceneAsset*)Payload); }

    ResourceDirectory directory;
    ThreadSystem      threads = nullptr;
};

SceneAssetLoader::SceneAssetLoader(ResourceDirectory CookedDirectory): Data(MemNew(SceneAssetLoaderData, CookedDirectory)) {}
SceneAssetLoader::~SceneAssetLoader() { MemDelete(Data); }

AssetLoader SceneAssetLoader::GetLoader()
{
    return { .UserData = Data,
             .pBegin = SceneAssetLoaderData::begin,
             .pPoll = SceneAssetLoaderData::poll,
             .pDestroyRequest = SceneAssetLoaderData::destroyRequest,
             .pDestroyPayload = SceneAssetLoaderData::destroyPayload };
}
} // namespace hz
