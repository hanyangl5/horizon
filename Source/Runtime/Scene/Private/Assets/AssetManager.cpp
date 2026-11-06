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
struct AssetSlot
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

struct AssetRecord
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
    ~AssetManagerData() { hmfree(pRecords); }

    AssetSlot* find(AssetHandle handle)
    {
        if (handle.index >= slots.size())
        {
            return nullptr;
        }
        AssetSlot& slot = slots[handle.index];
        return slot.pEntry && slot.generation == handle.generation ? &slot : nullptr;
    }

    const void* getPayload(AssetHandle handle, AssetType type)
    {
        const AssetSlot* pSlot = find(handle);
        return pSlot && pSlot->state == AssetState::Ready && pSlot->pEntry->type == type ? pSlot->pPayload : nullptr;
    }

    void queueRetirement(uint32_t index)
    {
        AssetSlot& slot = slots[index];
        if (slot.state == AssetState::Retiring && !slot.pRequest && !slot.uploading && !slot.retirementQueued)
        {
            slot.retirementQueued = true;
            retirements.pushBack(index);
        }
    }

    AssetError visit(AssetID id, AssetRequestPlan& plan)
    {
        const AssetRecord* pRecord = hmgetp_null(pRecords, id);
        if (pRecord)
        {
            return slots[pRecord->value.index].state == AssetState::Retiring ? AssetError::Retiring : AssetError::None;
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
        uint32_t index = slots.size();
        if (!freeSlots.empty())
        {
            index = freeSlots[freeSlots.size() - 1];
            freeSlots.popBack();
        }
        else
        {
            slots.pushBack({});
        }
        AssetSlot& slot = slots[index];
        slot.pEntry = &entry;
        slot.state = AssetState::Queued;
        if (entry.dependencies.count)
        {
            slot.pDependencies = (AssetHandle*)tf_malloc(entry.dependencies.count * sizeof(AssetHandle));
            for (uint32_t i = 0; i < entry.dependencies.count; ++i)
            {
                const AssetHandle dependency = hmgetp_null(pRecords, entry.dependencies.pData[i])->value;
                slot.pDependencies[i] = dependency;
                ++slots[dependency.index].references;
            }
        }
        const AssetRecord record = { .key = entry.id, .value = { .index = index, .generation = slot.generation } };
        hmputs(pRecords, record);
    }

    bool release(AssetHandle handle)
    {
        AssetSlot* pSlot = find(handle);
        if (!pSlot || !pSlot->references)
        {
            return false;
        }
        if (--pSlot->references == 0)
        {
            pSlot->state = AssetState::Retiring;
            // The payload left the ready set; work recorded until now may still reference it.
            pSlot->gpuUse = pContext ? pContext->getResourceUseId() : 0;
            ++tableRevision;
            queueRetirement(handle.index);
        }
        return true;
    }

    void destroyPayload(AssetSlot& slot)
    {
        if (slot.pPayload)
        {
            loaders[(uint32_t)slot.pEntry->type].pDestroyPayload(slot.pPayload, loaders[(uint32_t)slot.pEntry->type].pUserData);
            slot.pPayload = nullptr;
        }
    }

    void releaseDependencies(AssetSlot& slot)
    {
        if (!slot.pDependencies)
        {
            return;
        }
        for (uint32_t i = 0; i < slot.pEntry->dependencies.count; ++i)
        {
            release(slot.pDependencies[i]);
        }
        tf_free(slot.pDependencies);
        slot.pDependencies = nullptr;
    }

    void retire()
    {
        for (uint32_t i = 0; i < retirements.size();)
        {
            const uint32_t index = retirements[i];
            AssetSlot&     slot = slots[index];
            if (pContext && !pContext->isResourceUseComplete(slot.gpuUse))
            {
                ++i;
                continue;
            }
            retirements[i] = retirements[retirements.size() - 1];
            retirements.popBack();
            ASSERT(slot.state == AssetState::Retiring && !slot.references && !slot.pRequest && !slot.uploading);
            destroyPayload(slot);
            releaseDependencies(slot);
            hmdel(pRecords, slot.pEntry->id);
            const uint32_t generation = slot.generation + 1;
            slot = { .generation = generation ? generation : 1 };
            freeSlots.pushBack(index);
        }
    }

    void update(AssetSlot& slot)
    {
        if (!slot.pEntry || slot.state == AssetState::Ready)
        {
            return;
        }
        bool dependenciesReady = true;
        if (slot.state != AssetState::Retiring && slot.state != AssetState::Failed && slot.pDependencies)
        {
            for (uint32_t i = 0; i < slot.pEntry->dependencies.count; ++i)
            {
                const AssetState state = slots[slot.pDependencies[i].index].state;
                dependenciesReady &= state == AssetState::Ready;
                if (state == AssetState::Failed)
                {
                    slot.state = AssetState::Failed;
                    slot.error = AssetError::DependencyFailed;
                }
            }
        }
        const AssetLoader& loader = loaders[(uint32_t)slot.pEntry->type];
        if (slot.state == AssetState::Queued)
        {
            slot.pRequest =
                loader.pBegin(*slot.pEntry, { .index = (uint32_t)(&slot - slots.data()), .generation = slot.generation }, loader.pUserData);
            slot.state = slot.pRequest ? AssetState::LoadingCPU : AssetState::Failed;
            if (!slot.pRequest)
            {
                slot.error = AssetError::LoadFailed;
            }
        }
        if (slot.pRequest)
        {
            const AssetLoadResult result = loader.pPoll(slot.pRequest, loader.pUserData);
            if (result.status == AssetLoadStatus::Pending)
            {
                ASSERT(!result.pPayload);
                return;
            }
            loader.pDestroyRequest(slot.pRequest, loader.pUserData);
            slot.pRequest = nullptr;
            slot.pPayload = result.pPayload;
            if (slot.state == AssetState::Retiring)
            {
                queueRetirement((uint32_t)(&slot - slots.data()));
            }
            else if (slot.state != AssetState::Failed)
            {
                slot.state =
                    result.status == AssetLoadStatus::Succeeded && result.pPayload ? AssetState::WaitingDependencies : AssetState::Failed;
                if (slot.state == AssetState::Failed)
                {
                    slot.error = AssetError::LoadFailed;
                }
            }
        }
        if (slot.state == AssetState::WaitingDependencies && dependenciesReady)
        {
            if (loader.pUpload)
            {
                slot.uploading = true;
                slot.state = AssetState::UploadingGPU;
                loader.pUpload(slot.pPayload, assetManager, { slot.pDependencies, slot.pEntry->dependencies.count }, loader.pUserData);
            }
            else
            {
                slot.state = AssetState::Ready;
            }
        }
        if (slot.uploading)
        {
            const AssetLoadStatus status = loader.pPollUpload(slot.pPayload, loader.pUserData);
            if (status == AssetLoadStatus::Pending)
            {
                return;
            }
            slot.uploading = false;
            if (slot.state == AssetState::Retiring)
            {
                queueRetirement((uint32_t)(&slot - slots.data()));
            }
            else if (slot.state != AssetState::Failed)
            {
                slot.state = status == AssetLoadStatus::Succeeded ? AssetState::Ready : AssetState::Failed;
                if (slot.state == AssetState::Failed)
                {
                    slot.error = AssetError::UploadFailed;
                }
            }
        }
        if (slot.state == AssetState::Ready)
        {
            ++tableRevision;
        }
        if (slot.state == AssetState::Failed)
        {
            destroyPayload(slot);
            releaseDependencies(slot);
        }
    }

    const AssetCatalog&      catalog;
    const AssetManager&      assetManager;
    AssetLoader              loaders[4] = {};
    Array<AssetSlot>         slots;
    Array<uint32_t>          freeSlots;
    Array<uint32_t>          retirements;
    AssetRecord*             pRecords = nullptr;
    RenderContext*           pContext = nullptr;
    bool                     shuttingDown = false;
    uint64_t                 tableRevision = 1;
    uint64_t                 builtRevision = 0;
    Array<GpuMeshRecord>     meshes;
    Array<GpuVertexStream>   vertexStreams;
    Array<GpuSubmeshRecord>  submeshes;
    Array<GpuMaterialRecord> materials;
    Array<GpuTextureRecord>  textures;
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
    pData->freeSlots.reserve(pData->slots.size() + plan.entries.size());
    pData->retirements.reserve(pData->slots.size() + plan.entries.size());
    for (const AssetCatalogEntry* pEntry : plan.entries)
    {
        pData->create(*pEntry);
    }
    const AssetHandle handle = hmgetp_null(pData->pRecords, id)->value;
    ++pData->slots[handle.index].references;
    return handle;
}

bool AssetManager::release(AssetHandle handle) { return pData->release(handle); }

void AssetManager::update()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::update");
    pData->retire();
    for (AssetSlot& slot : pData->slots)
    {
        pData->update(slot);
    }
    pData->retire();
}

AssetState AssetManager::getState(AssetHandle handle) const
{
    const AssetSlot* pSlot = pData->find(handle);
    return pSlot ? pSlot->state : AssetState::Invalid;
}

AssetError AssetManager::getError(AssetHandle handle) const
{
    const AssetSlot* pSlot = pData->find(handle);
    return pSlot ? pSlot->error : AssetError::UnknownAsset;
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
    const AssetRecord* pRecord = hmgetp_null(pData->pRecords, id);
    return pRecord ? pRecord->value : AssetHandle{};
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
        data.meshes.resize(data.slots.size());
        data.materials.resize(data.slots.size());
        data.textures.resize(data.slots.size());
        for (uint32_t i = 0; i < data.slots.size(); ++i)
        {
            const AssetSlot& slot = data.slots[i];
            if (slot.state != AssetState::Ready)
            {
                continue;
            }
            if (slot.pEntry->type == AssetType::Texture)
            {
                data.textures[i] = { .id = { .index = i, .generation = slot.generation },
                                     .srv = ((const TextureAsset*)slot.pPayload)->getTexture().getSrvIndex() };
            }
            else if (slot.pEntry->type == AssetType::Mesh)
            {
                const MeshAsset& mesh = *(const MeshAsset*)slot.pPayload;
                data.meshes[i] = { .id = { .index = i, .generation = slot.generation },
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
            else if (slot.pEntry->type == AssetType::Material)
            {
                const SceneAssetMaterial& material = ((const MaterialAsset*)slot.pPayload)->getDescription();
                GpuMaterialRecord&        record = data.materials[i];
                record.id = { .index = i, .generation = slot.generation };
                const AssetID textureIDs[] = { material.baseColorTexture, material.normalTexture, material.metallicRoughnessTexture,
                                               material.emissiveTexture };
                GpuTextureID* ppTextures[] = { &record.baseColorTexture, &record.normalTexture, &record.metallicRoughnessTexture,
                                               &record.emissiveTexture };
                for (uint32_t texture = 0; texture < 4; ++texture)
                {
                    if (!textureIDs[texture].isValid())
                    {
                        continue;
                    }
                    const AssetHandle handle = find(textureIDs[texture]);
                    ASSERT(getTexture(handle));
                    *ppTextures[texture] = { .index = handle.index, .generation = handle.generation };
                }
                memcpy(record.baseColorFactor, material.baseColorFactor, sizeof(record.baseColorFactor));
                memcpy(record.emissiveFactor, material.emissiveFactor, sizeof(record.emissiveFactor));
                record.metallicFactor = material.metallicFactor;
                record.roughnessFactor = material.roughnessFactor;
                record.alphaCutoff = material.alphaCutoff;
                record.alphaMode = (uint32_t)material.alphaMode;
                record.doubleSided = material.doubleSided;
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

uint32_t AssetManager::getCount() const { return (uint32_t)hmlen(pData->pRecords); }

void AssetManager::shutdown()
{
    if (pData->shuttingDown)
    {
        return;
    }
    pData->shuttingDown = true;
    for (AssetSlot& slot : pData->slots)
    {
        if (!slot.pEntry)
        {
            continue;
        }
        if (slot.pRequest)
        {
            const AssetLoader& loader = pData->loaders[(uint32_t)slot.pEntry->type];
            loader.pDestroyRequest(slot.pRequest, loader.pUserData);
            slot.pRequest = nullptr;
        }
        if (slot.uploading)
        {
            const AssetLoader& loader = pData->loaders[(uint32_t)slot.pEntry->type];
            loader.pWaitUpload(slot.pPayload, loader.pUserData);
            slot.uploading = false;
        }
    }
    // Shutdown happens between frames, so idling covers every submission that could reference a payload.
    if (pData->pContext)
    {
        pData->pContext->waitIdle();
    }
    pData->retirements.clear();
    for (AssetSlot& slot : pData->slots)
    {
        if (!slot.pEntry)
        {
            continue;
        }
        slot.references = 0;
        slot.state = AssetState::Retiring;
        slot.gpuUse = 0;
        slot.retirementQueued = false;
    }
    // Discard external references, retaining dependency edges so parents retire first.
    for (const AssetSlot& slot : pData->slots)
    {
        if (slot.pDependencies)
        {
            for (uint32_t i = 0; i < slot.pEntry->dependencies.count; ++i)
            {
                ++pData->slots[slot.pDependencies[i].index].references;
            }
        }
    }
    for (uint32_t i = 0; i < pData->slots.size(); ++i)
    {
        if (pData->slots[i].pEntry && !pData->slots[i].references)
        {
            pData->queueRetirement(i);
        }
    }
    pData->retire();
    ASSERT(!getCount());
}
} // namespace hz
