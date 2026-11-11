/* Copyright (c) 2026 Horizon */

#include "Scene/AssetManager.h"
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "Scene/MeshAsset.h"
#include "Scene/MaterialAsset.h"
#include "Scene/TextureAsset.h"

#include "Core/IMemory.h"
#include "Profiler/IProfiler.h"

namespace hz
{
struct ManagedAsset
{
    const AssetCatalogEntry* pEntry = nullptr;
    uint32_t                 generation = 1;
    uint32_t                 references = 0;
    AssetState               state = AssetState::Invalid;
    AssetError               error = AssetError::None;
    void*                    pRequest = nullptr;
    void*                    pPayload = nullptr;
    AssetHandle*             pDependencies = nullptr;
    uint64_t                 gpuUse = 0;
    bool                     uploading = false;
    bool                     retirementQueued = false;
};

struct AssetIndex
{
    AssetID     key;
    AssetHandle value;
};

struct AssetVisit
{
    AssetID key;
    bool    complete;
};

struct AssetTraversal
{
    const AssetCatalogEntry* pEntry;
    uint32_t                 nextDependency = 0;
};

class AssetRequestPlan
{
public:
    ~AssetRequestPlan() { hmfree(pVisits); }
    Array<AssetTraversal>           stack;
    Array<const AssetCatalogEntry*> entries;
    AssetVisit*                     pVisits = nullptr;
};

class AssetManagerData
{
public:
    AssetManagerData(const AssetCatalog& catalog, const AssetManager& assetManager, RenderContext* pContext):
        catalog(catalog), assetManager(assetManager), pContext(pContext)
    {
    }
    ~AssetManagerData() { hmfree(pIndices); }

    ManagedAsset* find(AssetHandle handle)
    {
        if (handle.index >= assets.size())
        {
            return nullptr;
        }
        ManagedAsset& asset = assets[handle.index];
        return asset.pEntry && asset.generation == handle.generation ? &asset : nullptr;
    }

    const void* getPayload(AssetHandle handle, AssetType type)
    {
        const ManagedAsset* pAsset = find(handle);
        return pAsset && pAsset->state == AssetState::Ready && pAsset->pEntry->type == type ? pAsset->pPayload : nullptr;
    }

    void queueRetirement(uint32_t index)
    {
        ManagedAsset& asset = assets[index];
        if (asset.state == AssetState::Retiring && !asset.pRequest && !asset.uploading && !asset.retirementQueued)
        {
            asset.retirementQueued = true;
            retirements.pushBack(index);
        }
    }

    AssetError visit(AssetID id, AssetRequestPlan& plan)
    {
        const AssetIndex* pIndex = hmgetp_null(pIndices, id);
        if (pIndex)
        {
            return assets[pIndex->value.index].state == AssetState::Retiring ? AssetError::Retiring : AssetError::None;
        }
        const AssetVisit* pVisit = hmgetp_null(plan.pVisits, id);
        if (pVisit)
        {
            return pVisit->complete ? AssetError::None : AssetError::DependencyCycle;
        }
        const AssetCatalogEntry* pEntry = catalog.find(id);
        if (!pEntry)
        {
            return AssetError::UnknownAsset;
        }
        if (!loaders[(uint32_t)pEntry->type].pBegin)
        {
            return AssetError::MissingLoader;
        }
        const AssetVisit visit = { .key = id };
        hmputs(plan.pVisits, visit);
        plan.stack.pushBack({ .pEntry = pEntry });
        return AssetError::None;
    }

    AssetError prepare(AssetID id, AssetRequestPlan& plan)
    {
        AssetError error = visit(id, plan);
        while (error == AssetError::None && !plan.stack.empty())
        {
            AssetTraversal& traversal = plan.stack[plan.stack.size() - 1];
            if (traversal.nextDependency < traversal.pEntry->dependencies.count)
            {
                error = visit(traversal.pEntry->dependencies.pData[traversal.nextDependency++], plan);
            }
            else
            {
                hmgetp_null(plan.pVisits, traversal.pEntry->id)->complete = true;
                plan.entries.pushBack(traversal.pEntry);
                plan.stack.popBack();
            }
        }
        return error;
    }

    void create(const AssetCatalogEntry& entry)
    {
        uint32_t index = assets.size();
        if (!freeIndices.empty())
        {
            index = freeIndices[freeIndices.size() - 1];
            freeIndices.popBack();
        }
        else
        {
            assets.pushBack({});
        }
        ManagedAsset& asset = assets[index];
        asset.pEntry = &entry;
        asset.state = AssetState::Queued;
        if (entry.dependencies.count)
        {
            asset.pDependencies = (AssetHandle*)tf_malloc(entry.dependencies.count * sizeof(AssetHandle));
            for (uint32_t i = 0; i < entry.dependencies.count; ++i)
            {
                const AssetHandle dependency = hmgetp_null(pIndices, entry.dependencies.pData[i])->value;
                asset.pDependencies[i] = dependency;
                ++assets[dependency.index].references;
            }
        }
        const AssetIndex mapping = { .key = entry.id, .value = { .index = index, .generation = asset.generation } };
        hmputs(pIndices, mapping);
    }

    bool release(AssetHandle handle)
    {
        ManagedAsset* pAsset = find(handle);
        if (!pAsset || !pAsset->references)
        {
            return false;
        }
        if (--pAsset->references == 0)
        {
            pAsset->state = AssetState::Retiring;
            // The payload left the ready set; work recorded until now may still reference it.
            pAsset->gpuUse = pContext ? pContext->getResourceUseId() : 0;
            ++tableRevision;
            queueRetirement(handle.index);
        }
        return true;
    }

    void destroyPayload(ManagedAsset& asset)
    {
        if (asset.pPayload)
        {
            loaders[(uint32_t)asset.pEntry->type].pDestroyPayload(asset.pPayload, loaders[(uint32_t)asset.pEntry->type].pUserData);
            asset.pPayload = nullptr;
        }
    }

    void releaseDependencies(ManagedAsset& asset)
    {
        if (!asset.pDependencies)
        {
            return;
        }
        for (uint32_t i = 0; i < asset.pEntry->dependencies.count; ++i)
        {
            release(asset.pDependencies[i]);
        }
        tf_free(asset.pDependencies);
        asset.pDependencies = nullptr;
    }

    void retire()
    {
        for (uint32_t i = 0; i < retirements.size();)
        {
            const uint32_t index = retirements[i];
            ManagedAsset&  asset = assets[index];
            if (pContext && !pContext->isResourceUseComplete(asset.gpuUse))
            {
                ++i;
                continue;
            }
            retirements[i] = retirements[retirements.size() - 1];
            retirements.popBack();
            ASSERT(asset.state == AssetState::Retiring && !asset.references && !asset.pRequest && !asset.uploading);
            destroyPayload(asset);
            releaseDependencies(asset);
            hmdel(pIndices, asset.pEntry->id);
            const uint32_t generation = asset.generation + 1;
            asset = { .generation = generation ? generation : 1 };
            freeIndices.pushBack(index);
        }
    }

    void update(ManagedAsset& asset)
    {
        if (!asset.pEntry || asset.state == AssetState::Ready)
        {
            return;
        }
        bool dependenciesReady = true;
        if (asset.state != AssetState::Retiring && asset.state != AssetState::Failed && asset.pDependencies)
        {
            for (uint32_t i = 0; i < asset.pEntry->dependencies.count; ++i)
            {
                const AssetState state = assets[asset.pDependencies[i].index].state;
                dependenciesReady &= state == AssetState::Ready;
                if (state == AssetState::Failed)
                {
                    asset.state = AssetState::Failed;
                    asset.error = AssetError::DependencyFailed;
                }
            }
        }
        const AssetLoader& loader = loaders[(uint32_t)asset.pEntry->type];
        if (asset.state == AssetState::Queued)
        {
            asset.pRequest = loader.pBegin(*asset.pEntry, { .index = (uint32_t)(&asset - assets.data()), .generation = asset.generation },
                                           loader.pUserData);
            asset.state = asset.pRequest ? AssetState::LoadingCPU : AssetState::Failed;
            if (!asset.pRequest)
            {
                asset.error = AssetError::LoadFailed;
            }
        }
        if (asset.pRequest)
        {
            const AssetLoadResult result = loader.pPoll(asset.pRequest, loader.pUserData);
            if (result.status == AssetLoadStatus::Pending)
            {
                ASSERT(!result.pPayload);
                return;
            }
            loader.pDestroyRequest(asset.pRequest, loader.pUserData);
            asset.pRequest = nullptr;
            asset.pPayload = result.pPayload;
            if (asset.state == AssetState::Retiring)
            {
                queueRetirement((uint32_t)(&asset - assets.data()));
            }
            else if (asset.state != AssetState::Failed)
            {
                asset.state =
                    result.status == AssetLoadStatus::Succeeded && result.pPayload ? AssetState::WaitingDependencies : AssetState::Failed;
                if (asset.state == AssetState::Failed)
                {
                    asset.error = AssetError::LoadFailed;
                }
            }
        }
        if (asset.state == AssetState::WaitingDependencies && dependenciesReady)
        {
            if (loader.pUpload)
            {
                asset.uploading = true;
                asset.state = AssetState::UploadingGPU;
                loader.pUpload(asset.pPayload, assetManager, { asset.pDependencies, asset.pEntry->dependencies.count }, loader.pUserData);
            }
            else
            {
                asset.state = AssetState::Ready;
            }
        }
        if (asset.uploading)
        {
            const AssetLoadStatus status = loader.pPollUpload(asset.pPayload, loader.pUserData);
            if (status == AssetLoadStatus::Pending)
            {
                return;
            }
            asset.uploading = false;
            if (asset.state == AssetState::Retiring)
            {
                queueRetirement((uint32_t)(&asset - assets.data()));
            }
            else if (asset.state != AssetState::Failed)
            {
                asset.state = status == AssetLoadStatus::Succeeded ? AssetState::Ready : AssetState::Failed;
                if (asset.state == AssetState::Failed)
                {
                    asset.error = AssetError::UploadFailed;
                }
            }
        }
        if (asset.state == AssetState::Ready)
        {
            ++tableRevision;
        }
        if (asset.state == AssetState::Failed)
        {
            destroyPayload(asset);
            releaseDependencies(asset);
        }
    }

    const AssetCatalog&      catalog;
    const AssetManager&      assetManager;
    AssetLoader              loaders[4] = {};
    Array<ManagedAsset>      assets;
    Array<uint32_t>          freeIndices;
    Array<uint32_t>          retirements;
    AssetIndex*              pIndices = nullptr;
    RenderContext*           pContext = nullptr;
    bool                     shuttingDown = false;
    uint64_t                 tableRevision = 1;
    uint64_t                 builtRevision = 0;
    Array<GpuMeshData>       meshes;
    Array<GpuVertexStream>   vertexStreams;
    Array<GpuSubmeshData>    submeshes;
    Array<GpuMaterialData>   materials;
    Array<GpuTextureData>    textures;
};

AssetManager::AssetManager(const AssetCatalog& catalog, RenderContext* pContext): pData(tf_new(AssetManagerData, catalog, *this, pContext))
{
}
AssetManager::~AssetManager()
{
    shutdown();
    tf_delete(pData);
}

void AssetManager::setLoader(AssetType type, const AssetLoader& loader)
{
    ASSERT(type <= AssetType::Texture && !getCount() && !pData->shuttingDown);
    ASSERT(loader.pBegin && loader.pPoll && loader.pDestroyRequest && loader.pDestroyPayload);
    ASSERT((loader.pUpload && loader.pPollUpload && loader.pWaitUpload) || (!loader.pUpload && !loader.pPollUpload && !loader.pWaitUpload));
    pData->loaders[(uint32_t)type] = loader;
}

AssetHandle AssetManager::request(AssetID id, AssetError* pError)
{
    AssetRequestPlan plan;
    const AssetError error = pData->shuttingDown ? AssetError::ShuttingDown : pData->prepare(id, plan);
    if (pError)
    {
        *pError = error;
    }
    if (error != AssetError::None)
    {
        char text[kIDStringCapacity];
        id.toString(text);
        LOGF(eERROR, "Asset request '%s' rejected (error %u)", text, (uint32_t)error);
        return {};
    }
    pData->freeIndices.reserve(pData->assets.size() + plan.entries.size());
    pData->retirements.reserve(pData->assets.size() + plan.entries.size());
    for (const AssetCatalogEntry* pEntry : plan.entries)
    {
        pData->create(*pEntry);
    }
    const AssetHandle handle = hmgetp_null(pData->pIndices, id)->value;
    ++pData->assets[handle.index].references;
    return handle;
}

bool AssetManager::release(AssetHandle handle) { return pData->release(handle); }

void AssetManager::update()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::update");
    pData->retire();
    for (ManagedAsset& asset : pData->assets)
    {
        pData->update(asset);
    }
    pData->retire();
}

AssetState AssetManager::getState(AssetHandle handle) const
{
    const ManagedAsset* pAsset = pData->find(handle);
    return pAsset ? pAsset->state : AssetState::Invalid;
}

AssetError AssetManager::getError(AssetHandle handle) const
{
    const ManagedAsset* pAsset = pData->find(handle);
    return pAsset ? pAsset->error : AssetError::UnknownAsset;
}

const SceneAsset* AssetManager::getScene(AssetHandle handle) const
{
    return (const SceneAsset*)pData->getPayload(handle, AssetType::Scene);
}

const TextureAsset* AssetManager::getTexture(AssetHandle handle) const
{
    return (const TextureAsset*)pData->getPayload(handle, AssetType::Texture);
}
const MeshAsset* AssetManager::getMesh(AssetHandle handle) const { return (const MeshAsset*)pData->getPayload(handle, AssetType::Mesh); }
const MaterialAsset* AssetManager::getMaterial(AssetHandle handle) const
{
    return (const MaterialAsset*)pData->getPayload(handle, AssetType::Material);
}

AssetHandle AssetManager::find(AssetID id) const
{
    const AssetIndex* pIndex = hmgetp_null(pData->pIndices, id);
    return pIndex ? pIndex->value : AssetHandle{};
}

GpuAssetTables AssetManager::getGpuTables()
{
    AssetManagerData& data = *pData;
    if (data.builtRevision != data.tableRevision)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::encodeGpuTables");
        data.meshes.clear();
        data.vertexStreams.clear();
        data.submeshes.clear();
        data.materials.clear();
        data.textures.clear();
        data.meshes.resize(data.assets.size());
        data.materials.resize(data.assets.size());
        data.textures.resize(data.assets.size());
        for (uint32_t i = 0; i < data.assets.size(); ++i)
        {
            const ManagedAsset& asset = data.assets[i];
            if (asset.state != AssetState::Ready)
            {
                continue;
            }
            if (asset.pEntry->type == AssetType::Texture)
            {
                data.textures[i] = { .id = { .index = i, .generation = asset.generation },
                                     .srv = ((const TextureAsset*)asset.pPayload)->getTexture().getSrvIndex() };
            }
            else if (asset.pEntry->type == AssetType::Mesh)
            {
                const MeshAsset& mesh = *(const MeshAsset*)asset.pPayload;
                data.meshes[i] = { .id = { .index = i, .generation = asset.generation },
                                   .indexSrv = mesh.getIndexBuffer().getSrvIndex(),
                                   .indexStride = mesh.getIndexType() == INDEX_TYPE_UINT16 ? 2u : 4u,
                                   .vertexStreamOffset = data.vertexStreams.size(),
                                   .vertexStreamCount = mesh.getVertexBufferCount(),
                                   .submeshOffset = data.submeshes.size(),
                                   .submeshCount = mesh.getDescription().submeshes.count };
                for (uint32_t stream = 0; stream < mesh.getVertexBufferCount(); ++stream)
                {
                    data.vertexStreams.pushBack(
                        { .srv = mesh.getVertexBuffer(stream).getSrvIndex(), .stride = mesh.getVertexStride(stream) });
                }
                for (uint32_t draw = 0; draw < mesh.getDescription().submeshes.count; ++draw)
                {
                    const IndirectDrawIndexArguments& args = mesh.getDraw(draw);
                    data.submeshes.pushBack(
                        { .indexCount = args.indexCount, .firstIndex = args.startIndex, .vertexOffset = (int32_t)args.vertexOffset });
                }
            }
            else if (asset.pEntry->type == AssetType::Material)
            {
                const SceneAssetMaterial& material = ((const MaterialAsset*)asset.pPayload)->getDescription();
                GpuMaterialData&          mapping = data.materials[i];
                mapping.id = { .index = i, .generation = asset.generation };
                const AssetID textureIDs[] = { material.baseColorTexture, material.normalTexture, material.metallicRoughnessTexture,
                                               material.emissiveTexture };
                uint32_t*     ppTextures[] = { &mapping.baseColorTexture, &mapping.normalTexture, &mapping.metallicRoughnessTexture,
                                               &mapping.emissiveTexture };
                for (uint32_t texture = 0; texture < 4; ++texture)
                {
                    if (!textureIDs[texture].isValid())
                    {
                        continue;
                    }
                    const AssetHandle   handle = find(textureIDs[texture]);
                    const TextureAsset* textureAsset = getTexture(handle);
                    ASSERT(textureAsset);
                    *ppTextures[texture] = textureAsset->getTexture().getSrvIndex();
                }
                memcpy(mapping.baseColorFactor, material.baseColorFactor, sizeof(mapping.baseColorFactor));
                memcpy(mapping.emissiveFactor, material.emissiveFactor, sizeof(mapping.emissiveFactor));
                mapping.metallicFactor = material.metallicFactor;
                mapping.roughnessFactor = material.roughnessFactor;
                mapping.alphaCutoff = material.alphaCutoff;
                mapping.alphaMode = (uint32_t)material.alphaMode;
                mapping.doubleSided = material.doubleSided;
            }
        }
        data.builtRevision = data.tableRevision;
    }
    return { .meshes = { data.meshes.data(), data.meshes.size() },
             .vertexStreams = { data.vertexStreams.data(), data.vertexStreams.size() },
             .submeshes = { data.submeshes.data(), data.submeshes.size() },
             .materials = { data.materials.data(), data.materials.size() },
             .textures = { data.textures.data(), data.textures.size() },
             .revision = data.builtRevision };
}

uint32_t AssetManager::getCount() const { return (uint32_t)hmlen(pData->pIndices); }

void AssetManager::shutdown()
{
    if (pData->shuttingDown)
    {
        return;
    }
    pData->shuttingDown = true;
    for (ManagedAsset& asset : pData->assets)
    {
        if (!asset.pEntry)
        {
            continue;
        }
        if (asset.pRequest)
        {
            const AssetLoader& loader = pData->loaders[(uint32_t)asset.pEntry->type];
            loader.pDestroyRequest(asset.pRequest, loader.pUserData);
            asset.pRequest = nullptr;
        }
        if (asset.uploading)
        {
            const AssetLoader& loader = pData->loaders[(uint32_t)asset.pEntry->type];
            loader.pWaitUpload(asset.pPayload, loader.pUserData);
            asset.uploading = false;
        }
    }
    // Shutdown happens between frames, so idling covers every submission that could reference a payload.
    if (pData->pContext)
    {
        pData->pContext->waitIdle();
    }
    pData->retirements.clear();
    for (ManagedAsset& asset : pData->assets)
    {
        if (!asset.pEntry)
        {
            continue;
        }
        asset.references = 0;
        asset.state = AssetState::Retiring;
        asset.gpuUse = 0;
        asset.retirementQueued = false;
    }
    // Discard external references, retaining dependency edges so parents retire first.
    for (const ManagedAsset& asset : pData->assets)
    {
        if (asset.pDependencies)
        {
            for (uint32_t i = 0; i < asset.pEntry->dependencies.count; ++i)
            {
                ++pData->assets[asset.pDependencies[i].index].references;
            }
        }
    }
    for (uint32_t i = 0; i < pData->assets.size(); ++i)
    {
        if (pData->assets[i].pEntry && !pData->assets[i].references)
        {
            pData->queueRetirement(i);
        }
    }
    pData->retire();
    ASSERT(!getCount());
}
} // namespace hz
