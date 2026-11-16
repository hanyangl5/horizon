/* Copyright (c) 2026 Horizon */

#include "Scene/AssetManager.h"
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "Scene/MeshAsset.h"
#include "Scene/MaterialAsset.h"
#include "Scene/TextureAsset.h"

#include <stdio.h>

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
    bool                     tableDirty = false;
    bool                     tableEncoded = false;
    char                     diagnostic[192] = {};
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

    ManagedAsset* find(AssetHandle Handle)
    {
        if (Handle.index >= assets.Size())
        {
            return nullptr;
        }
        ManagedAsset& asset = assets[Handle.index];
        return asset.pEntry && asset.generation == Handle.generation ? &asset : nullptr;
    }

    const void* getPayload(AssetHandle Handle, AssetType Type)
    {
        const ManagedAsset* pAsset = find(Handle);
        return pAsset && pAsset->state == AssetState::Ready && pAsset->pEntry->Type == Type ? pAsset->pPayload : nullptr;
    }

    void queueRetirement(uint32_t index)
    {
        ManagedAsset& asset = assets[index];
        if (asset.state == AssetState::Retiring && !asset.pRequest && !asset.uploading && !asset.retirementQueued)
        {
            asset.retirementQueued = true;
            retirements.PushBack(index);
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
        if (!loaders[(uint32_t)pEntry->Type].pBegin)
        {
            return AssetError::MissingLoader;
        }
        const AssetVisit visit = { .key = id };
        hmputs(plan.pVisits, visit);
        plan.stack.PushBack({ .pEntry = pEntry });
        return AssetError::None;
    }

    AssetError prepare(AssetID id, AssetRequestPlan& plan)
    {
        AssetError error = visit(id, plan);
        while (error == AssetError::None && !plan.stack.Empty())
        {
            AssetTraversal& traversal = plan.stack[plan.stack.Size() - 1];
            if (traversal.nextDependency < traversal.pEntry->dependencies.Count)
            {
                error = visit(traversal.pEntry->dependencies.Data[traversal.nextDependency++], plan);
            }
            else
            {
                hmgetp_null(plan.pVisits, traversal.pEntry->id)->complete = true;
                plan.entries.PushBack(traversal.pEntry);
                plan.stack.PopBack();
            }
        }
        return error;
    }

    void create(const AssetCatalogEntry& entry)
    {
        uint32_t index = assets.Size();
        if (!freeIndices.Empty())
        {
            index = freeIndices[freeIndices.Size() - 1];
            freeIndices.PopBack();
        }
        else
        {
            assets.PushBack({});
        }
        ManagedAsset& asset = assets[index];
        asset.pEntry = &entry;
        asset.state = AssetState::Queued;
        if (entry.dependencies.Count)
        {
            asset.pDependencies = (AssetHandle*)MemAllocate(entry.dependencies.Count * sizeof(AssetHandle));
            for (uint32_t i = 0; i < entry.dependencies.Count; ++i)
            {
                const AssetHandle dependency = hmgetp_null(pIndices, entry.dependencies.Data[i])->value;
                asset.pDependencies[i] = dependency;
                ++assets[dependency.index].references;
            }
        }
        const AssetIndex mapping = { .key = entry.id, .value = { .index = index, .generation = asset.generation } };
        hmputs(pIndices, mapping);
        pending.PushBack(index);
    }

    bool release(AssetHandle Handle)
    {
        ManagedAsset* pAsset = find(Handle);
        if (!pAsset || !pAsset->references)
        {
            return false;
        }
        if (--pAsset->references == 0)
        {
            pAsset->state = AssetState::Retiring;
            // The payload left the ready set; work recorded until now may still reference it.
            pAsset->gpuUse = pContext ? pContext->getResourceUseId() : 0;
            markTableDirty(Handle.index);
            queueRetirement(Handle.index);
        }
        return true;
    }

    void destroyPayload(ManagedAsset& asset)
    {
        if (asset.pPayload)
        {
            loaders[(uint32_t)asset.pEntry->Type].pDestroyPayload(asset.pPayload, loaders[(uint32_t)asset.pEntry->Type].UserData);
            asset.pPayload = nullptr;
        }
    }

    void releaseDependencies(ManagedAsset& asset)
    {
        if (!asset.pDependencies)
        {
            return;
        }
        for (uint32_t i = 0; i < asset.pEntry->dependencies.Count; ++i)
        {
            release(asset.pDependencies[i]);
        }
        MemDeallocate(asset.pDependencies);
        asset.pDependencies = nullptr;
    }

    void retire()
    {
        for (uint32_t i = 0; i < retirements.Size();)
        {
            const uint32_t index = retirements[i];
            ManagedAsset&  asset = assets[index];
            if (pContext && !pContext->isResourceUseComplete(asset.gpuUse))
            {
                ++i;
                continue;
            }
            retirements[i] = retirements[retirements.Size() - 1];
            retirements.PopBack();
            ASSERT(asset.state == AssetState::Retiring && !asset.references && !asset.pRequest && !asset.uploading);
            destroyPayload(asset);
            releaseDependencies(asset);
            hmdel(pIndices, asset.pEntry->id);
            // A release already drops the entry; only a slot that was still encoded needs clearing here.
            if (asset.tableEncoded)
                markTableDirty(index);
            const uint32_t generation = asset.generation + 1;
            asset = { .generation = generation ? generation : 1 };
            freeIndices.PushBack(index);
        }
    }

    // Records the failure and its reason together so callers can query the diagnostic immediately.
    void fail(ManagedAsset& asset, AssetError error, const char* pReason)
    {
        asset.state = AssetState::Failed;
        asset.error = error;
        if (pReason && pReason[0])
            snprintf(asset.diagnostic, sizeof(asset.diagnostic), "%s", pReason);
        else if (!asset.diagnostic[0])
            snprintf(asset.diagnostic, sizeof(asset.diagnostic), "Cannot load %s", asset.pEntry->location.pPath);
    }

    // Returns true while the asset still has work to poll; Ready, Failed and retired slots drop out.
    bool update(ManagedAsset& asset)
    {
        const uint32_t index = (uint32_t)(&asset - assets.GetData());
        if (!asset.pEntry || asset.state == AssetState::Ready)
        {
            return false;
        }
        bool dependenciesReady = true;
        if (asset.state != AssetState::Retiring && asset.state != AssetState::Failed && asset.pDependencies)
        {
            for (uint32_t i = 0; i < asset.pEntry->dependencies.Count; ++i)
            {
                const AssetState state = assets[asset.pDependencies[i].index].state;
                dependenciesReady &= state == AssetState::Ready;
                if (state == AssetState::Failed)
                    fail(asset, AssetError::DependencyFailed, "A dependency failed to load");
            }
        }
        const AssetLoader& loader = loaders[(uint32_t)asset.pEntry->Type];
        if (asset.state == AssetState::Queued)
        {
            asset.pRequest = loader.pBegin(*asset.pEntry, { .index = index, .generation = asset.generation }, loader.UserData);
            if (!asset.pRequest)
                fail(asset, AssetError::LoadFailed, "The loader rejected the asset");
            else
                asset.state = AssetState::LoadingCPU;
        }
        if (asset.pRequest)
        {
            const AssetLoadResult result = loader.pPoll(asset.pRequest, loader.UserData);
            if (result.status == AssetLoadStatus::Pending)
            {
                ASSERT(!result.pPayload);
                return true;
            }
            loader.pDestroyRequest(asset.pRequest, loader.UserData);
            asset.pRequest = nullptr;
            asset.pPayload = result.pPayload;
            if (asset.state == AssetState::Retiring)
            {
                queueRetirement(index);
            }
            else if (asset.state != AssetState::Failed)
            {
                if (result.status == AssetLoadStatus::Succeeded && result.pPayload)
                    asset.state = AssetState::WaitingDependencies;
                else
                    fail(asset, AssetError::LoadFailed, result.pError);
            }
        }
        if (asset.state == AssetState::WaitingDependencies && dependenciesReady)
        {
            if (loader.pUpload)
            {
                asset.uploading = true;
                asset.state = AssetState::UploadingGPU;
                loader.pUpload(asset.pPayload, assetManager, { asset.pDependencies, asset.pEntry->dependencies.Count }, loader.UserData);
            }
            else
            {
                asset.state = AssetState::Ready;
            }
        }
        if (asset.uploading)
        {
            const AssetLoadStatus status = loader.pPollUpload(asset.pPayload, loader.UserData);
            if (status == AssetLoadStatus::Pending)
            {
                return true;
            }
            asset.uploading = false;
            if (asset.state == AssetState::Retiring)
            {
                queueRetirement(index);
            }
            else if (asset.state != AssetState::Failed)
            {
                if (status == AssetLoadStatus::Succeeded)
                    asset.state = AssetState::Ready;
                else
                    fail(asset, AssetError::UploadFailed, nullptr);
            }
        }
        if (asset.state == AssetState::Ready)
        {
            markTableDirty(index);
            return false;
        }
        if (asset.state == AssetState::Failed)
        {
            if (!asset.diagnostic[0])
                snprintf(asset.diagnostic, sizeof(asset.diagnostic), "Cannot load %s", asset.pEntry->location.pPath);
            destroyPayload(asset);
            releaseDependencies(asset);
            markTableDirty(index);
            return false;
        }
        if (asset.state == AssetState::Retiring && !asset.pRequest && !asset.uploading)
        {
            queueRetirement(index);
            return false;
        }
        return true;
    }

    // Marks one asset for the next GPU table update. Materials bake texture Descriptors into their entry,
    // so any texture change invalidates every material.
    void markTableDirty(uint32_t index)
    {
        ManagedAsset& asset = assets[index];
        if (asset.tableDirty)
            return;
        asset.tableDirty = true;
        tableDirty.PushBack(index);
        ++tableRevision;
        if (asset.pEntry && asset.pEntry->Type == AssetType::Texture)
        {
            for (uint32_t i = 0; i < assets.Size(); ++i)
            {
                if (assets[i].pEntry && assets[i].pEntry->Type == AssetType::Material)
                    markTableDirty(i);
            }
        }
    }

    void clearTableEntry(uint32_t index)
    {
        // Vertex stream and submesh ranges are append only; a full rebuild is what reclaims them.
        tableGarbage += meshes[index].submeshCount + meshes[index].vertexStreamCount;
        meshes[index] = {};
        materials[index] = {};
        textures[index] = {};
        assets[index].tableDirty = false;
        assets[index].tableEncoded = false;
    }

    void encodeTableEntry(uint32_t index)
    {
        const ManagedAsset& asset = assets[index];
        if (asset.state != AssetState::Ready || !asset.pPayload)
        {
            clearTableEntry(index);
            return;
        }
        if (asset.pEntry->Type == AssetType::Texture)
        {
            textures[index] = { .id = { .index = index, .generation = asset.generation },
                                .srv = ((const TextureAsset*)asset.pPayload)->getTexture().GetSrvIndex() };
        }
        else if (asset.pEntry->Type == AssetType::Mesh)
        {
            const MeshAsset& mesh = *(const MeshAsset*)asset.pPayload;
            tableGarbage += meshes[index].submeshCount + meshes[index].vertexStreamCount;
            meshes[index] = { .id = { .index = index, .generation = asset.generation },
                              .indexSrv = mesh.getIndexBuffer().GetSrvIndex(),
                              .indexStride = mesh.getIndexType() == IndexTypeUInt16 ? 2u : 4u,
                              .vertexStreamOffset = vertexStreams.Size(),
                              .vertexStreamCount = mesh.getVertexBufferCount(),
                              .submeshOffset = submeshes.Size(),
                              .submeshCount = mesh.getDescription().submeshes.Count };
            for (uint32_t stream = 0; stream < mesh.getVertexBufferCount(); ++stream)
            {
                vertexStreams.PushBack({ .srv = mesh.getVertexBuffer(stream).GetSrvIndex(), .stride = mesh.getVertexStride(stream) });
            }
            for (uint32_t Draw = 0; Draw < mesh.getDescription().submeshes.Count; ++Draw)
            {
                const IndirectDrawIndexArguments& args = mesh.getDraw(Draw);
                submeshes.PushBack({ .indexCount = args.indexCount, .firstIndex = args.startIndex,
                                     .vertexOffset = (int32_t)args.vertexOffset });
            }
        }
        else if (asset.pEntry->Type == AssetType::Material)
        {
            const SceneAssetMaterial& material = ((const MaterialAsset*)asset.pPayload)->getDescription();
            GpuMaterialData&          mapping = materials[index];
            mapping.id = { .index = index, .generation = asset.generation };
            const AssetID textureIDs[] = { material.baseColorTexture, material.normalTexture, material.metallicRoughnessTexture,
                                           material.emissiveTexture };
            uint32_t*     ppTextures[] = { &mapping.baseColorTexture, &mapping.normalTexture, &mapping.metallicRoughnessTexture,
                                           &mapping.emissiveTexture };
            for (uint32_t texture = 0; texture < 4; ++texture)
            {
                if (!textureIDs[texture].IsValid())
                {
                    continue;
                }
                const AssetIndex*   pIndex = hmgetp_null(pIndices, textureIDs[texture]);
                const ManagedAsset* pTexture = pIndex ? &assets[pIndex->value.index] : nullptr;
                // Only a released texture lease can leave this Empty; keep the previous mapping instead of crashing.
                if (pTexture && pTexture->state == AssetState::Ready && pTexture->pPayload)
                    *ppTextures[texture] = ((const TextureAsset*)pTexture->pPayload)->getTexture().GetSrvIndex();
            }
            memcpy(mapping.baseColorFactor, material.baseColorFactor, sizeof(mapping.baseColorFactor));
            memcpy(mapping.emissiveFactor, material.emissiveFactor, sizeof(mapping.emissiveFactor));
            mapping.metallicFactor = material.metallicFactor;
            mapping.roughnessFactor = material.roughnessFactor;
            mapping.alphaCutoff = material.alphaCutoff;
            mapping.alphaMode = (uint32_t)material.alphaMode;
            mapping.doubleSided = material.doubleSided;
        }
        assets[index].tableDirty = false;
        assets[index].tableEncoded = true;
    }

    void encodeTables()
    {
        meshes.Resize(assets.Size());
        materials.Resize(assets.Size());
        textures.Resize(assets.Size());
        if (tableGarbage > kTableGarbageLimit)
        {
            vertexStreams.Clear();
            submeshes.Clear();
            tableGarbage = 0;
            for (uint32_t i = 0; i < assets.Size(); ++i)
            {
                meshes[i] = {};
                materials[i] = {};
                textures[i] = {};
                assets[i].tableDirty = false;
            }
            for (uint32_t i = 0; i < assets.Size(); ++i)
            {
                if (assets[i].state == AssetState::Ready && assets[i].pPayload)
                    encodeTableEntry(i);
            }
            tableDirty.Clear();
            return;
        }
        for (uint32_t index : tableDirty)
            encodeTableEntry(index);
        tableDirty.Clear();
    }

    static constexpr uint32_t kTableGarbageLimit = 4096;

    const AssetCatalog&      catalog;
    const AssetManager&      assetManager;
    AssetLoader              loaders[4] = {};
    Array<ManagedAsset>      assets;
    Array<uint32_t>          pending;
    Array<uint32_t>          tableDirty;
    Array<uint32_t>          freeIndices;
    Array<uint32_t>          retirements;
    AssetIndex*              pIndices = nullptr;
    RenderContext*           pContext = nullptr;
    bool                     shuttingDown = false;
    uint64_t                 tableRevision = 1;
    uint64_t                 builtRevision = 0;
    uint32_t                 tableGarbage = 0;
    Array<GpuMeshData>       meshes;
    Array<GpuVertexStream>   vertexStreams;
    Array<GpuSubmeshData>    submeshes;
    Array<GpuMaterialData>   materials;
    Array<GpuTextureData>    textures;
};

AssetManager::AssetManager(const AssetCatalog& catalog, RenderContext* pContext): Data(MemNew(AssetManagerData, catalog, *this, pContext))
{
}
AssetManager::~AssetManager()
{
    shutdown();
    MemDelete(Data);
}

void AssetManager::setLoader(AssetType Type, const AssetLoader& loader)
{
    ASSERT(Type <= AssetType::Texture && !getCount() && !Data->shuttingDown);
    ASSERT(loader.pBegin && loader.pPoll && loader.pDestroyRequest && loader.pDestroyPayload);
    ASSERT((loader.pUpload && loader.pPollUpload && loader.pWaitUpload) || (!loader.pUpload && !loader.pPollUpload && !loader.pWaitUpload));
    Data->loaders[(uint32_t)Type] = loader;
}

AssetHandle AssetManager::request(AssetID id, AssetError* pError)
{
    AssetRequestPlan plan;
    const AssetError error = Data->shuttingDown ? AssetError::ShuttingDown : Data->prepare(id, plan);
    if (pError)
    {
        *pError = error;
    }
    if (error != AssetError::None)
    {
        char text[kIDStringCapacity];
        id.toString(text);
        LOGF(Error, "Asset request '%s' rejected (error %u)", text, (uint32_t)error);
        return {};
    }
    Data->freeIndices.Reserve(Data->assets.Size() + plan.entries.Size());
    Data->retirements.Reserve(Data->assets.Size() + plan.entries.Size());
    for (const AssetCatalogEntry* pEntry : plan.entries)
    {
        Data->create(*pEntry);
    }
    const AssetHandle Handle = hmgetp_null(Data->pIndices, id)->value;
    ++Data->assets[Handle.index].references;
    return Handle;
}

bool AssetManager::release(AssetHandle Handle) { return Data->release(Handle); }

void AssetManager::update()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::update");
    AssetManagerData& data = *Data;
    data.retire();
    // Only assets with work in flight are polled. A slot freed while still listed drops out here.
    uint32_t kept = 0;
    for (uint32_t i = 0; i < data.pending.Size(); ++i)
    {
        if (data.update(data.assets[data.pending[i]]))
            data.pending[kept++] = data.pending[i];
    }
    data.pending.Resize(kept);
    data.retire();
}

AssetState AssetManager::getState(AssetHandle Handle) const
{
    const ManagedAsset* pAsset = Data->find(Handle);
    return pAsset ? pAsset->state : AssetState::Invalid;
}

AssetError AssetManager::getError(AssetHandle Handle) const
{
    const ManagedAsset* pAsset = Data->find(Handle);
    return pAsset ? pAsset->error : AssetError::UnknownAsset;
}

const char* AssetManager::getDiagnostic(AssetHandle Handle) const
{
    const ManagedAsset* pAsset = Data->find(Handle);
    return pAsset ? pAsset->diagnostic : "";
}

const SceneAsset* AssetManager::getScene(AssetHandle Handle) const
{
    return (const SceneAsset*)Data->getPayload(Handle, AssetType::Scene);
}

const TextureAsset* AssetManager::getTexture(AssetHandle Handle) const
{
    return (const TextureAsset*)Data->getPayload(Handle, AssetType::Texture);
}
const MeshAsset* AssetManager::getMesh(AssetHandle Handle) const { return (const MeshAsset*)Data->getPayload(Handle, AssetType::Mesh); }
const MaterialAsset* AssetManager::getMaterial(AssetHandle Handle) const
{
    return (const MaterialAsset*)Data->getPayload(Handle, AssetType::Material);
}

AssetHandle AssetManager::find(AssetID id) const
{
    const AssetIndex* pIndex = hmgetp_null(Data->pIndices, id);
    return pIndex ? pIndex->value : AssetHandle{};
}

GpuAssetTables AssetManager::getGpuTables()
{
    AssetManagerData& data = *Data;
    if (data.builtRevision != data.tableRevision)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::encodeGpuTables");
        data.encodeTables();
        data.builtRevision = data.tableRevision;
    }
    return { .meshes = { data.meshes.GetData(), data.meshes.Size() },
             .vertexStreams = { data.vertexStreams.GetData(), data.vertexStreams.Size() },
             .submeshes = { data.submeshes.GetData(), data.submeshes.Size() },
             .materials = { data.materials.GetData(), data.materials.Size() },
             .textures = { data.textures.GetData(), data.textures.Size() },
             .revision = data.builtRevision };
}

uint32_t AssetManager::getCount() const { return (uint32_t)hmlen(Data->pIndices); }

void AssetManager::shutdown()
{
    if (Data->shuttingDown)
    {
        return;
    }
    Data->shuttingDown = true;
    Data->pending.Clear();
    for (ManagedAsset& asset : Data->assets)
    {
        if (!asset.pEntry)
        {
            continue;
        }
        if (asset.pRequest)
        {
            const AssetLoader& loader = Data->loaders[(uint32_t)asset.pEntry->Type];
            loader.pDestroyRequest(asset.pRequest, loader.UserData);
            asset.pRequest = nullptr;
        }
        if (asset.uploading)
        {
            const AssetLoader& loader = Data->loaders[(uint32_t)asset.pEntry->Type];
            loader.pWaitUpload(asset.pPayload, loader.UserData);
            asset.uploading = false;
        }
    }
    // Shutdown happens between frames, so idling covers every submission that could reference a payload.
    if (Data->pContext)
    {
        Data->pContext->WaitIdle();
    }
    Data->retirements.Clear();
    for (ManagedAsset& asset : Data->assets)
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
    for (const ManagedAsset& asset : Data->assets)
    {
        if (asset.pDependencies)
        {
            for (uint32_t i = 0; i < asset.pEntry->dependencies.Count; ++i)
            {
                ++Data->assets[asset.pDependencies[i].index].references;
            }
        }
    }
    for (uint32_t i = 0; i < Data->assets.Size(); ++i)
    {
        if (Data->assets[i].pEntry && !Data->assets[i].references)
        {
            Data->queueRetirement(i);
        }
    }
    Data->retire();
    ASSERT(!getCount());
}
} // namespace hz
