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
    ~SceneLoadRequest() { tf_delete(pScene); }

    bool matchesDependencies() const
    {
        Array<AssetID> dependencies;
        dependencies.reserve(pScene->getNodes().count);
        for (const SceneAssetNode& node : pScene->getNodes())
        {
            if (node.mesh.isValid())
            {
                dependencies.pushBack(node.mesh);
            }
        }
        if (!dependencies.empty())
        {
            qsort(dependencies.data(), dependencies.size(), sizeof(AssetID), compareIDs);
        }
        uint32_t count = 0;
        for (uint32_t i = 0; i < dependencies.size(); ++i)
        {
            if (i && dependencies[i] == dependencies[i - 1])
            {
                continue;
            }
            if (count == entry.dependencies.count || dependencies[i] != entry.dependencies.pData[count++])
            {
                return false;
            }
        }
        return count == entry.dependencies.count;
    }

    bool decode()
    {
        FileStream stream = {};
        if (!fsOpenStreamFromPath(directory, entry.location.pPath, FM_READ, &stream))
        {
            LOGF(eERROR, "Cannot open SceneAsset '%s'", entry.location.pPath);
            return false;
        }
        const ssize_t size = fsGetStreamFileSize(&stream);
        if (size <= 0 || size > 64 * 1024 * 1024)
        {
            fsCloseStream(&stream);
            LOGF(eERROR, "Invalid SceneAsset file size for '%s'", entry.location.pPath);
            return false;
        }
        Array<char>  json((uint32_t)size);
        const size_t bytesRead = fsReadFromStream(&stream, json.data(), (size_t)size);
        fsCloseStream(&stream);
        if (bytesRead != (size_t)size)
        {
            LOGF(eERROR, "Cannot read SceneAsset '%s'", entry.location.pPath);
            return false;
        }
        pScene = tf_new(SceneAsset);
        if (!pScene->parse({ json.data(), json.size() }) || pScene->getID() != entry.id)
        {
            LOGF(eERROR, "Invalid SceneAsset payload or mismatched identity in '%s'", entry.location.pPath);
            return false;
        }
        if (!matchesDependencies())
        {
            LOGF(eERROR, "SceneAsset dependencies differ from Catalog in '%s'", entry.location.pPath);
            return false;
        }
        return true;
    }

    static void execute(void* pUser, uint64_t)
    {
        SceneLoadRequest*     pRequest = (SceneLoadRequest*)pUser;
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
        {
            LOGF(eERROR, "Cannot start SceneAsset loader worker");
        }
    }
    ~SceneAssetLoaderData()
    {
        if (threads)
        {
            threadSystemExit(&threads, &gThreadSystemExitDescDefault);
        }
    }

    static void* begin(const AssetCatalogEntry& entry, AssetHandle, void* pUser)
    {
        SceneAssetLoaderData* pLoader = (SceneAssetLoaderData*)pUser;
        if (!pLoader->threads || entry.type != AssetType::Scene || entry.location.root != AssetPathRoot::Cooked ||
            entry.location.elementIndex != UINT32_MAX)
        {
            LOGF(eERROR, "Cannot schedule SceneAsset '%s'", entry.location.pPath);
            return nullptr;
        }
        SceneLoadRequest* pRequest = tf_new(SceneLoadRequest, entry, pLoader->directory);
        threadSystemAddTask(pLoader->threads, SceneLoadRequest::execute, pRequest);
        return pRequest;
    }

    static AssetLoadResult poll(void* pJob, void*)
    {
        SceneLoadRequest*     pRequest = (SceneLoadRequest*)pJob;
        const AssetLoadStatus status = (AssetLoadStatus)tfrg_atomic32_load_acquire(&pRequest->status);
        if (status == AssetLoadStatus::Pending)
        {
            return {};
        }
        SceneAsset* pScene = pRequest->pScene;
        pRequest->pScene = nullptr;
        return { .status = status, .pPayload = pScene };
    }

    static void destroyRequest(void* pJob, void* pUser)
    {
        SceneLoadRequest* pRequest = (SceneLoadRequest*)pJob;
        if ((AssetLoadStatus)tfrg_atomic32_load_acquire(&pRequest->status) == AssetLoadStatus::Pending)
        {
            threadSystemWaitIdle(((SceneAssetLoaderData*)pUser)->threads);
        }
        tf_delete(pRequest);
    }

    static void destroyPayload(void* pPayload, void*) { tf_delete((SceneAsset*)pPayload); }

    ResourceDirectory directory;
    ThreadSystem      threads = nullptr;
};

SceneAssetLoader::SceneAssetLoader(ResourceDirectory cookedDirectory): pData(tf_new(SceneAssetLoaderData, cookedDirectory)) {}
SceneAssetLoader::~SceneAssetLoader() { tf_delete(pData); }

AssetLoader SceneAssetLoader::getLoader()
{
    return { .pUserData = pData,
             .pBegin = SceneAssetLoaderData::begin,
             .pPoll = SceneAssetLoaderData::poll,
             .pDestroyRequest = SceneAssetLoaderData::destroyRequest,
             .pDestroyPayload = SceneAssetLoaderData::destroyPayload };
}
} // namespace hz
