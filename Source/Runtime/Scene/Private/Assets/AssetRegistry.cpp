/* Copyright (c) 2026 Horizon */

#include "Scene/AssetRegistry.h"
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "Scene/MeshAsset.h"
#include "Scene/MaterialAsset.h"
#include "Scene/TextureAsset.h"

#include "Core/IMemory.h"

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
    uint32_t                 snapshotReferences = 0;
    bool                     uploading = false;
    bool                     retirementQueued = false;
};

struct SnapshotAsset
{
    const void* pPayload = nullptr;
    uint32_t    generation = 0;
    AssetType   type = AssetType::Scene;
};

class AssetSnapshotData
{
public:
    const void* find(AssetHandle handle, AssetType type) const
    {
        if (handle.index >= assets.size())
        {
            return nullptr;
        }
        const SnapshotAsset& asset = assets[handle.index];
        return asset.generation == handle.generation && asset.type == type ? asset.pPayload : nullptr;
    }
    AssetRegistryData*       pOwner = nullptr;
    Array<SnapshotAsset>     assets;
    Array<GpuMeshRecord>     meshes;
    Array<GpuVertexStream>   vertexStreams;
    Array<GpuSubmeshRecord>  submeshes;
    Array<GpuMaterialRecord> materials;
    Array<GpuTextureRecord>  textures;
    uint64_t                 revision = 0;
    bool                     gpuTables = false;
    uint32_t                 references = 1;
};

struct SnapshotSubmission
{
    AssetSnapshotData* pSnapshot;
    RenderContext*     pContext;
    SubmitHandle       submission;
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

class AssetRegistryData
{
public:
    AssetRegistryData(const AssetCatalog& catalog, const AssetRegistry& registry): catalog(catalog), registry(registry)
    {
        submissions.reserve(16);
    }
    ~AssetRegistryData() { hmfree(pRecords); }

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
        if (slot.state == AssetState::Retiring && !slot.pRequest && !slot.uploading && !slot.snapshotReferences && !slot.retirementQueued)
        {
            slot.retirementQueued = true;
            retirements.pushBack(index);
        }
    }

    void releaseSnapshot(AssetSnapshotData* pSnapshot)
    {
        if (--pSnapshot->references)
        {
            return;
        }
        for (uint32_t i = 0; i < pSnapshot->assets.size(); ++i)
        {
            if (pSnapshot->assets[i].pPayload)
            {
                ASSERT(slots[i].snapshotReferences);
                --slots[i].snapshotReferences;
                queueRetirement(i);
            }
        }
        tf_delete(pSnapshot);
        --snapshotCount;
    }

    void invalidateSnapshot()
    {
        ++snapshotRevision;
        if (pCurrentSnapshot)
        {
            releaseSnapshot(pCurrentSnapshot);
            pCurrentSnapshot = nullptr;
        }
    }

    void updateSubmissions(bool wait)
    {
        for (uint32_t i = 0; i < submissions.size();)
        {
            SnapshotSubmission& pending = submissions[i];
            if (wait)
            {
                pending.pContext->wait(pending.submission);
            }
            if (wait || pending.pContext->isComplete(pending.submission))
            {
                releaseSnapshot(pending.pSnapshot);
                pending = submissions[submissions.size() - 1];
                submissions.popBack();
            }
            else
            {
                ++i;
            }
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
            invalidateSnapshot();
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
        while (!retirements.empty())
        {
            const uint32_t index = retirements[retirements.size() - 1];
            retirements.popBack();
            AssetSlot& slot = slots[index];
            ASSERT(slot.state == AssetState::Retiring && !slot.references && !slot.pRequest && !slot.uploading && !slot.snapshotReferences);
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
                loader.pUpload(slot.pPayload, registry, { slot.pDependencies, slot.pEntry->dependencies.count }, loader.pUserData);
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
            invalidateSnapshot();
        }
        if (slot.state == AssetState::Failed)
        {
            destroyPayload(slot);
            releaseDependencies(slot);
        }
    }

    const AssetCatalog&       catalog;
    const AssetRegistry&      registry;
    AssetLoader               loaders[4] = {};
    Array<AssetSlot>          slots;
    Array<uint32_t>           freeSlots;
    Array<uint32_t>           retirements;
    AssetRecord*              pRecords = nullptr;
    bool                      shuttingDown = false;
    AssetSnapshotData*        pCurrentSnapshot = nullptr;
    uint32_t                  snapshotCount = 0;
    uint64_t                  snapshotRevision = 1;
    Array<SnapshotSubmission> submissions;
};

AssetRegistry::AssetRegistry(const AssetCatalog& catalog): pData(tf_new(AssetRegistryData, catalog, *this)) {}
AssetRegistry::~AssetRegistry()
{
    shutdown();
    tf_delete(pData);
}

void AssetRegistry::setLoader(AssetType type, const AssetLoader& loader)
{
    ASSERT(type <= AssetType::Texture && !getCount() && !pData->shuttingDown);
    ASSERT(loader.pBegin && loader.pPoll && loader.pDestroyRequest && loader.pDestroyPayload);
    ASSERT((loader.pUpload && loader.pPollUpload && loader.pWaitUpload) || (!loader.pUpload && !loader.pPollUpload && !loader.pWaitUpload));
    pData->loaders[(uint32_t)type] = loader;
}

AssetHandle AssetRegistry::request(AssetID id, AssetError* pError)
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

bool AssetRegistry::release(AssetHandle handle) { return pData->release(handle); }

void AssetRegistry::update()
{
    pData->updateSubmissions(false);
    pData->retire();
    for (AssetSlot& slot : pData->slots)
    {
        pData->update(slot);
    }
    pData->retire();
}

AssetState AssetRegistry::getState(AssetHandle handle) const
{
    const AssetSlot* pSlot = pData->find(handle);
    return pSlot ? pSlot->state : AssetState::Invalid;
}

AssetError AssetRegistry::getError(AssetHandle handle) const
{
    const AssetSlot* pSlot = pData->find(handle);
    return pSlot ? pSlot->error : AssetError::UnknownAsset;
}

const SceneAsset* AssetRegistry::getScene(AssetHandle handle) const
{
    return (const SceneAsset*)pData->getPayload(handle, AssetType::Scene);
}

const TextureAsset* AssetRegistry::getTexture(AssetHandle handle) const
{
    return (const TextureAsset*)pData->getPayload(handle, AssetType::Texture);
}
const MeshAsset* AssetRegistry::getMesh(AssetHandle handle) const { return (const MeshAsset*)pData->getPayload(handle, AssetType::Mesh); }
const MaterialAsset* AssetRegistry::getMaterial(AssetHandle handle) const
{
    return (const MaterialAsset*)pData->getPayload(handle, AssetType::Material);
}

AssetHandle AssetRegistry::find(AssetID id) const
{
    const AssetRecord* pRecord = hmgetp_null(pData->pRecords, id);
    return pRecord ? pRecord->value : AssetHandle{};
}

AssetSnapshot AssetRegistry::captureSnapshot()
{
    AssetSnapshot result;
    if (pData->shuttingDown)
    {
        return result;
    }
    if (!pData->pCurrentSnapshot)
    {
        pData->pCurrentSnapshot = tf_new(AssetSnapshotData);
        pData->pCurrentSnapshot->pOwner = pData;
        pData->pCurrentSnapshot->revision = pData->snapshotRevision;
        pData->pCurrentSnapshot->assets.resize(pData->slots.size());
        ++pData->snapshotCount;
        for (uint32_t i = 0; i < pData->slots.size(); ++i)
        {
            AssetSlot& slot = pData->slots[i];
            if (slot.state == AssetState::Ready)
            {
                pData->pCurrentSnapshot->assets[i] = { .pPayload = slot.pPayload,
                                                       .generation = slot.generation,
                                                       .type = slot.pEntry->type };
                ++slot.snapshotReferences;
            }
        }
    }
    result.pData = pData->pCurrentSnapshot;
    ++result.pData->references;
    return result;
}

AssetSnapshot AssetRegistry::captureRenderSnapshot()
{
    AssetSnapshot result = captureSnapshot();
    if (!result.isValid() || result.pData->gpuTables)
    {
        return result;
    }
    AssetSnapshotData& data = *result.pData;
    data.meshes.resize(data.assets.size());
    data.materials.resize(data.assets.size());
    data.textures.resize(data.assets.size());
    for (uint32_t i = 0; i < data.assets.size(); ++i)
    {
        const SnapshotAsset& asset = data.assets[i];
        if (!asset.pPayload)
        {
            continue;
        }
        if (asset.type == AssetType::Texture)
        {
            data.textures[i] = { .id = { .index = i, .generation = asset.generation },
                                 .srv = ((const TextureAsset*)asset.pPayload)->getTexture().getSrvIndex() };
        }
        else if (asset.type == AssetType::Mesh)
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
                data.vertexStreams.pushBack({ .srv = mesh.getVertexBuffer(stream).getSrvIndex(), .stride = mesh.getVertexStride(stream) });
            }
            for (uint32_t draw = 0; draw < mesh.getDescription().submeshes.count; ++draw)
            {
                const IndirectDrawIndexArguments& args = mesh.getDraw(draw);
                data.submeshes.pushBack(
                    { .indexCount = args.indexCount, .firstIndex = args.startIndex, .vertexOffset = (int32_t)args.vertexOffset });
            }
        }
        else if (asset.type == AssetType::Material)
        {
            const SceneAssetMaterial& material = ((const MaterialAsset*)asset.pPayload)->getDescription();
            GpuMaterialRecord&        record = data.materials[i];
            record.id = { .index = i, .generation = asset.generation };
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
                ASSERT(data.find(handle, AssetType::Texture));
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
    data.gpuTables = true;
    return result;
}

uint64_t              AssetSnapshot::getRevision() const { return pData ? pData->revision : 0; }
GpuAssetTableSnapshot AssetSnapshot::getGpuTables() const
{
    ASSERT(pData && pData->gpuTables);
    return { .meshes = { pData->meshes.data(), pData->meshes.size() },
             .vertexStreams = { pData->vertexStreams.data(), pData->vertexStreams.size() },
             .submeshes = { pData->submeshes.data(), pData->submeshes.size() },
             .materials = { pData->materials.data(), pData->materials.size() },
             .textures = { pData->textures.data(), pData->textures.size() } };
}

void AssetRegistry::retireSnapshot(AssetSnapshot&& snapshot, RenderContext& context, SubmitHandle submission)
{
    ASSERT(snapshot.pData && snapshot.pData->pOwner == pData && submission.isValid() && !pData->shuttingDown);
    pData->submissions.pushBack({ .pSnapshot = snapshot.pData, .pContext = &context, .submission = submission });
    snapshot.pData = nullptr;
}

AssetSnapshot::~AssetSnapshot()
{
    if (pData)
    {
        pData->pOwner->releaseSnapshot(pData);
    }
}
AssetSnapshot::AssetSnapshot(AssetSnapshot&& other) noexcept: pData(other.pData) { other.pData = nullptr; }
AssetSnapshot& AssetSnapshot::operator=(AssetSnapshot&& other) noexcept
{
    if (this != &other)
    {
        if (pData)
        {
            pData->pOwner->releaseSnapshot(pData);
        }
        pData = other.pData;
        other.pData = nullptr;
    }
    return *this;
}
const SceneAsset* AssetSnapshot::getScene(AssetHandle handle) const
{
    return pData ? (const SceneAsset*)pData->find(handle, AssetType::Scene) : nullptr;
}
const TextureAsset* AssetSnapshot::getTexture(AssetHandle handle) const
{
    return pData ? (const TextureAsset*)pData->find(handle, AssetType::Texture) : nullptr;
}
const MeshAsset* AssetSnapshot::getMesh(AssetHandle handle) const
{
    return pData ? (const MeshAsset*)pData->find(handle, AssetType::Mesh) : nullptr;
}
const MaterialAsset* AssetSnapshot::getMaterial(AssetHandle handle) const
{
    return pData ? (const MaterialAsset*)pData->find(handle, AssetType::Material) : nullptr;
}

uint32_t AssetRegistry::getCount() const { return (uint32_t)hmlen(pData->pRecords); }

void AssetRegistry::shutdown()
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
    pData->updateSubmissions(true);
    pData->invalidateSnapshot();
    ASSERT(!pData->snapshotCount);
    pData->retirements.clear();
    for (AssetSlot& slot : pData->slots)
    {
        if (!slot.pEntry)
        {
            continue;
        }
        slot.references = 0;
        slot.state = AssetState::Retiring;
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
