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
    AssetManagerData(const AssetCatalog& Catalog, const AssetManager& AssetManagerValue, RenderContext* Context):
        catalog(Catalog), assetManager(AssetManagerValue), pContext(Context)
    {
    }
    ~AssetManagerData() { hmfree(pIndices); }

    ManagedAsset* Find(AssetHandle Handle)
    {
        if (Handle.index >= assets.Size())
        {
            return nullptr;
        }
        ManagedAsset& AssetValue = assets[Handle.index];
        return AssetValue.pEntry && AssetValue.generation == Handle.generation ? &AssetValue : nullptr;
    }

    const void* getPayload(AssetHandle Handle, AssetType Type)
    {
        const ManagedAsset* AssetPointer = Find(Handle);
        return AssetPointer && AssetPointer->state == AssetState::Ready && AssetPointer->pEntry->Type == Type ? AssetPointer->pPayload
                                                                                                              : nullptr;
    }

    void queueRetirement(uint32_t IndexValue)
    {
        ManagedAsset& AssetValue = assets[IndexValue];
        if (AssetValue.state == AssetState::Retiring && !AssetValue.pRequest && !AssetValue.uploading && !AssetValue.retirementQueued)
        {
            AssetValue.retirementQueued = true;
            retirements.PushBack(IndexValue);
        }
    }

    AssetError visit(AssetID Id, AssetRequestPlan& Plan)
    {
        const AssetIndex* IndexPointer = hmgetp_null(pIndices, Id);
        if (IndexPointer)
        {
            return assets[IndexPointer->value.index].state == AssetState::Retiring ? AssetError::Retiring : AssetError::None;
        }
        const AssetVisit* VisitPointer = hmgetp_null(Plan.pVisits, Id);
        if (VisitPointer)
        {
            return VisitPointer->complete ? AssetError::None : AssetError::DependencyCycle;
        }
        const AssetCatalogEntry* EntryPointer = catalog.Find(Id);
        if (!EntryPointer)
        {
            return AssetError::UnknownAsset;
        }
        if (!loaders[(uint32_t)EntryPointer->Type].pBegin)
        {
            return AssetError::MissingLoader;
        }
        const AssetVisit VisitValue = { .key = Id };
        hmputs(Plan.pVisits, VisitValue);
        Plan.stack.PushBack({ .pEntry = EntryPointer });
        return AssetError::None;
    }

    AssetError prepare(AssetID Id, AssetRequestPlan& Plan)
    {
        AssetError ErrorValue = visit(Id, Plan);
        while (ErrorValue == AssetError::None && !Plan.stack.Empty())
        {
            AssetTraversal& Traversal = Plan.stack[Plan.stack.Size() - 1];
            if (Traversal.nextDependency < Traversal.pEntry->dependencies.Count)
            {
                ErrorValue = visit(Traversal.pEntry->dependencies.Data[Traversal.nextDependency++], Plan);
            }
            else
            {
                hmgetp_null(Plan.pVisits, Traversal.pEntry->id)->complete = true;
                Plan.entries.PushBack(Traversal.pEntry);
                Plan.stack.PopBack();
            }
        }
        return ErrorValue;
    }

    void create(const AssetCatalogEntry& EntryValue)
    {
        uint32_t IndexValue = assets.Size();
        if (!freeIndices.Empty())
        {
            IndexValue = freeIndices[freeIndices.Size() - 1];
            freeIndices.PopBack();
        }
        else
        {
            assets.PushBack({});
        }
        ManagedAsset& AssetValue = assets[IndexValue];
        AssetValue.pEntry = &EntryValue;
        AssetValue.state = AssetState::Queued;
        if (EntryValue.dependencies.Count)
        {
            AssetValue.pDependencies = (AssetHandle*)MemAllocate(EntryValue.dependencies.Count * sizeof(AssetHandle));
            for (uint32_t I = 0; I < EntryValue.dependencies.Count; ++I)
            {
                const AssetHandle Dependency = hmgetp_null(pIndices, EntryValue.dependencies.Data[I])->value;
                AssetValue.pDependencies[I] = Dependency;
                ++assets[Dependency.index].references;
            }
        }
        const AssetIndex Mapping = { .key = EntryValue.id, .value = { .index = IndexValue, .generation = AssetValue.generation } };
        hmputs(pIndices, Mapping);
        pending.PushBack(IndexValue);
    }

    bool Release(AssetHandle Handle)
    {
        ManagedAsset* AssetPointer = Find(Handle);
        if (!AssetPointer || !AssetPointer->references)
        {
            return false;
        }
        if (--AssetPointer->references == 0)
        {
            AssetPointer->state = AssetState::Retiring;
            // The payload left the ready Set; work recorded until now may still reference it.
            AssetPointer->gpuUse = pContext ? pContext->GetResourceUseId() : 0;
            markTableDirty(Handle.index);
            queueRetirement(Handle.index);
        }
        return true;
    }

    void destroyPayload(ManagedAsset& AssetValue)
    {
        if (AssetValue.pPayload)
        {
            loaders[(uint32_t)AssetValue.pEntry->Type].pDestroyPayload(AssetValue.pPayload,
                                                                       loaders[(uint32_t)AssetValue.pEntry->Type].UserData);
            AssetValue.pPayload = nullptr;
        }
    }

    void releaseDependencies(ManagedAsset& AssetValue)
    {
        if (!AssetValue.pDependencies)
        {
            return;
        }
        for (uint32_t I = 0; I < AssetValue.pEntry->dependencies.Count; ++I)
        {
            Release(AssetValue.pDependencies[I]);
        }
        MemDeallocate(AssetValue.pDependencies);
        AssetValue.pDependencies = nullptr;
    }

    void retire()
    {
        for (uint32_t I = 0; I < retirements.Size();)
        {
            const uint32_t IndexValue = retirements[I];
            ManagedAsset&  AssetValue = assets[IndexValue];
            if (pContext && !pContext->IsResourceUseComplete(AssetValue.gpuUse))
            {
                ++I;
                continue;
            }
            retirements[I] = retirements[retirements.Size() - 1];
            retirements.PopBack();
            ASSERT(AssetValue.state == AssetState::Retiring && !AssetValue.references && !AssetValue.pRequest && !AssetValue.uploading);
            destroyPayload(AssetValue);
            releaseDependencies(AssetValue);
            hmdel(pIndices, AssetValue.pEntry->id);
            // A Release already drops the entry; only a slot that was still encoded needs clearing here.
            if (AssetValue.tableEncoded)
            {
                markTableDirty(IndexValue);
            }
            const uint32_t Generation = AssetValue.generation + 1;
            AssetValue = { .generation = Generation ? Generation : 1 };
            freeIndices.PushBack(IndexValue);
        }
    }

    // Records the failure and its reason together so callers can query the diagnostic immediately.
    void fail(ManagedAsset& AssetValue, AssetError ErrorValue, const char* Reason)
    {
        AssetValue.state = AssetState::Failed;
        AssetValue.error = ErrorValue;
        if (Reason && Reason[0])
        {
            snprintf(AssetValue.diagnostic, sizeof(AssetValue.diagnostic), "%s", Reason);
        }
        else if (!AssetValue.diagnostic[0])
        {
            snprintf(AssetValue.diagnostic, sizeof(AssetValue.diagnostic), "Cannot Load %s", AssetValue.pEntry->location.pPath);
        }
    }

    // Returns true while the asset still has work to poll; Ready, Failed and retired slots drop out.
    bool Update(ManagedAsset& AssetValue)
    {
        const uint32_t IndexValue = (uint32_t)(&AssetValue - assets.GetData());
        if (!AssetValue.pEntry || AssetValue.state == AssetState::Ready)
        {
            return false;
        }
        bool DependenciesReady = true;
        if (AssetValue.state != AssetState::Retiring && AssetValue.state != AssetState::Failed && AssetValue.pDependencies)
        {
            for (uint32_t I = 0; I < AssetValue.pEntry->dependencies.Count; ++I)
            {
                const ManagedAsset& Dependency = assets[AssetValue.pDependencies[I].index];
                // A material keeps its factors when a texture fails: the GPU table leaves that slot unbound.
                const bool Optional = AssetValue.pEntry->Type == AssetType::Material && Dependency.pEntry->Type == AssetType::Texture;
                if (Dependency.state == AssetState::Failed && Optional)
                {
                    continue;
                }
                DependenciesReady &= Dependency.state == AssetState::Ready;
                if (Dependency.state == AssetState::Failed)
                {
                    fail(AssetValue, AssetError::DependencyFailed, "A dependency failed to Load");
                }
            }
        }
        const AssetLoader& Loader = loaders[(uint32_t)AssetValue.pEntry->Type];
        if (AssetValue.state == AssetState::Queued)
        {
            AssetValue.pRequest =
                Loader.pBegin(*AssetValue.pEntry, { .index = IndexValue, .generation = AssetValue.generation }, Loader.UserData);
            if (!AssetValue.pRequest)
            {
                fail(AssetValue, AssetError::LoadFailed, "The loader rejected the asset");
            }
            else
            {
                AssetValue.state = AssetState::LoadingCPU;
            }
        }
        if (AssetValue.pRequest)
        {
            const AssetLoadResult Result = Loader.pPoll(AssetValue.pRequest, Loader.UserData);
            if (Result.status == AssetLoadStatus::Pending)
            {
                ASSERT(!Result.pPayload);
                return true;
            }
            Loader.pDestroyRequest(AssetValue.pRequest, Loader.UserData);
            AssetValue.pRequest = nullptr;
            AssetValue.pPayload = Result.pPayload;
            if (AssetValue.state == AssetState::Retiring)
            {
                queueRetirement(IndexValue);
            }
            else if (AssetValue.state != AssetState::Failed)
            {
                if (Result.status == AssetLoadStatus::Succeeded && Result.pPayload)
                {
                    AssetValue.state = AssetState::WaitingDependencies;
                }
                else
                {
                    fail(AssetValue, AssetError::LoadFailed, Result.pError);
                }
            }
        }
        if (AssetValue.state == AssetState::WaitingDependencies && DependenciesReady)
        {
            if (Loader.pUpload)
            {
                AssetValue.uploading = true;
                AssetValue.state = AssetState::UploadingGPU;
                Loader.pUpload(AssetValue.pPayload, assetManager, { AssetValue.pDependencies, AssetValue.pEntry->dependencies.Count },
                               Loader.UserData);
            }
            else
            {
                AssetValue.state = AssetState::Ready;
            }
        }
        if (AssetValue.uploading)
        {
            const AssetLoadStatus Status = Loader.pPollUpload(AssetValue.pPayload, Loader.UserData);
            if (Status == AssetLoadStatus::Pending)
            {
                return true;
            }
            AssetValue.uploading = false;
            if (AssetValue.state == AssetState::Retiring)
            {
                queueRetirement(IndexValue);
            }
            else if (AssetValue.state != AssetState::Failed)
            {
                if (Status == AssetLoadStatus::Succeeded)
                {
                    AssetValue.state = AssetState::Ready;
                }
                else
                {
                    fail(AssetValue, AssetError::UploadFailed, nullptr);
                }
            }
        }
        if (AssetValue.state == AssetState::Ready)
        {
            markTableDirty(IndexValue);
            return false;
        }
        if (AssetValue.state == AssetState::Failed)
        {
            if (!AssetValue.diagnostic[0])
            {
                snprintf(AssetValue.diagnostic, sizeof(AssetValue.diagnostic), "Cannot Load %s", AssetValue.pEntry->location.pPath);
            }
            destroyPayload(AssetValue);
            releaseDependencies(AssetValue);
            markTableDirty(IndexValue);
            return false;
        }
        if (AssetValue.state == AssetState::Retiring && !AssetValue.pRequest && !AssetValue.uploading)
        {
            queueRetirement(IndexValue);
            return false;
        }
        return true;
    }

    // Marks one asset for the next GPU table Update. Materials bake texture Descriptors into their entry,
    // so any texture change invalidates every material.
    void markTableDirty(uint32_t IndexValue)
    {
        ManagedAsset& AssetValue = assets[IndexValue];
        if (AssetValue.tableDirty)
        {
            return;
        }
        AssetValue.tableDirty = true;
        tableDirty.PushBack(IndexValue);
        ++tableRevision;
        if (AssetValue.pEntry && AssetValue.pEntry->Type == AssetType::Texture)
        {
            for (uint32_t I = 0; I < assets.Size(); ++I)
            {
                if (assets[I].pEntry && assets[I].pEntry->Type == AssetType::Material)
                {
                    markTableDirty(I);
                }
            }
        }
    }

    void clearTableEntry(uint32_t IndexValue)
    {
        // Vertex stream and submesh ranges are append only; a full rebuild is what reclaims them.
        tableGarbage += meshes[IndexValue].submeshCount + meshes[IndexValue].vertexStreamCount;
        meshes[IndexValue] = {};
        materials[IndexValue] = {};
        textures[IndexValue] = {};
        assets[IndexValue].tableDirty = false;
        assets[IndexValue].tableEncoded = false;
    }

    void encodeTableEntry(uint32_t IndexValue)
    {
        const ManagedAsset& AssetValue = assets[IndexValue];
        if (AssetValue.state != AssetState::Ready || !AssetValue.pPayload)
        {
            clearTableEntry(IndexValue);
            return;
        }
        if (AssetValue.pEntry->Type == AssetType::Texture)
        {
            textures[IndexValue] = { .id = { .index = IndexValue, .generation = AssetValue.generation },
                                     .srv = ((const TextureAsset*)AssetValue.pPayload)->GetTexture().GetSrvIndex() };
        }
        else if (AssetValue.pEntry->Type == AssetType::Mesh)
        {
            const MeshAsset& MeshValue = *(const MeshAsset*)AssetValue.pPayload;
            tableGarbage += meshes[IndexValue].submeshCount + meshes[IndexValue].vertexStreamCount;
            meshes[IndexValue] = { .id = { .index = IndexValue, .generation = AssetValue.generation },
                                   .indexSrv = MeshValue.GetIndexBuffer().GetSrvIndex(),
                                   .indexStride = MeshValue.GetIndexType() == INDEX_TYPE_UINT16 ? 2u : 4u,
                                   .vertexStreamOffset = vertexStreams.Size(),
                                   .vertexStreamCount = MeshValue.GetVertexBufferCount(),
                                   .submeshOffset = submeshes.Size(),
                                   .submeshCount = MeshValue.GetDescription().submeshes.Count };
            for (uint32_t Stream = 0; Stream < MeshValue.GetVertexBufferCount(); ++Stream)
            {
                const GPUBuffer& BufferValue = MeshValue.GetVertexBuffer(Stream);
                vertexStreams.PushBack(
                    { .srv = BufferValue.IsValid() ? BufferValue.GetSrvIndex() : UINT32_MAX, .stride = MeshValue.GetVertexStride(Stream) });
            }
            for (uint32_t Draw = 0; Draw < MeshValue.GetDescription().submeshes.Count; ++Draw)
            {
                const IndirectDrawIndexArguments& Args = MeshValue.GetDraw(Draw);
                submeshes.PushBack(
                    { .indexCount = Args.indexCount, .firstIndex = Args.startIndex, .vertexOffset = (int32_t)Args.vertexOffset });
            }
        }
        else if (AssetValue.pEntry->Type == AssetType::Material)
        {
            const SceneAssetMaterial& MaterialValue = ((const MaterialAsset*)AssetValue.pPayload)->GetDescription();
            GpuMaterialData&          Mapping = materials[IndexValue];
            Mapping.Id = { .index = IndexValue, .generation = AssetValue.generation };
            const AssetID TextureIDs[] = { MaterialValue.baseColorTexture, MaterialValue.normalTexture,
                                           MaterialValue.metallicRoughnessTexture, MaterialValue.emissiveTexture };
            uint32_t*     Textures[] = { &Mapping.BaseColorTexture, &Mapping.NormalTexture, &Mapping.MetallicRoughnessTexture,
                                         &Mapping.EmissiveTexture };
            for (uint32_t TextureValue = 0; TextureValue < 4; ++TextureValue)
            {
                if (!TextureIDs[TextureValue].IsValid())
                {
                    continue;
                }
                const AssetIndex*   IndexPointer = hmgetp_null(pIndices, TextureIDs[TextureValue]);
                const ManagedAsset* TexturePointer = IndexPointer ? &assets[IndexPointer->value.index] : nullptr;
                // Only a released texture lease can leave this Empty; keep the previous mapping instead of crashing.
                if (TexturePointer && TexturePointer->state == AssetState::Ready && TexturePointer->pPayload)
                {
                    *Textures[TextureValue] = ((const TextureAsset*)TexturePointer->pPayload)->GetTexture().GetSrvIndex();
                }
            }
            memcpy(Mapping.BaseColorFactor, MaterialValue.baseColorFactor, sizeof(Mapping.BaseColorFactor));
            memcpy(Mapping.EmissiveFactor, MaterialValue.emissiveFactor, sizeof(Mapping.EmissiveFactor));
            Mapping.MetallicFactor = MaterialValue.metallicFactor;
            Mapping.RoughnessFactor = MaterialValue.roughnessFactor;
            Mapping.AlphaCutoff = MaterialValue.alphaCutoff;
            Mapping.AlphaMode = (uint32_t)MaterialValue.alphaMode;
            Mapping.DoubleSided = MaterialValue.doubleSided;
            Mapping.Tags = MaterialValue.tags;
        }
        assets[IndexValue].tableDirty = false;
        assets[IndexValue].tableEncoded = true;
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
            for (uint32_t I = 0; I < assets.Size(); ++I)
            {
                meshes[I] = {};
                materials[I] = {};
                textures[I] = {};
                assets[I].tableDirty = false;
            }
            for (uint32_t I = 0; I < assets.Size(); ++I)
            {
                if (assets[I].state == AssetState::Ready && assets[I].pPayload)
                {
                    encodeTableEntry(I);
                }
            }
            tableDirty.Clear();
            return;
        }
        for (uint32_t IndexValue : tableDirty)
        {
            encodeTableEntry(IndexValue);
        }
        tableDirty.Clear();
    }

    static constexpr uint32_t kTableGarbageLimit = 4096;

    const AssetCatalog&      catalog;
    const AssetManager&      assetManager;
    AssetLoader              loaders[(uint32_t)AssetType::AnimationClip + 1] = {};
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

AssetManager::AssetManager(const AssetCatalog& Catalog, RenderContext* Context): Data(MemNew(AssetManagerData, Catalog, *this, Context)) {}
AssetManager::~AssetManager()
{
    Shutdown();
    MemDelete(Data);
}

void AssetManager::SetLoader(AssetType Type, const AssetLoader& Loader)
{
    ASSERT(Type <= AssetType::AnimationClip && !GetCount() && !Data->shuttingDown);
    ASSERT(Loader.pBegin && Loader.pPoll && Loader.pDestroyRequest && Loader.pDestroyPayload);
    ASSERT((Loader.pUpload && Loader.pPollUpload && Loader.pWaitUpload) || (!Loader.pUpload && !Loader.pPollUpload && !Loader.pWaitUpload));
    Data->loaders[(uint32_t)Type] = Loader;
}

AssetHandle AssetManager::Request(AssetID Id, AssetError* ErrorPointer)
{
    AssetRequestPlan Plan;
    const AssetError ErrorValue = Data->shuttingDown ? AssetError::ShuttingDown : Data->prepare(Id, Plan);
    if (ErrorPointer)
    {
        *ErrorPointer = ErrorValue;
    }
    if (ErrorValue != AssetError::None)
    {
        char Text[kIDStringCapacity];
        Id.ToString(Text);
        LOGF(Error, "Asset Request '%s' rejected (error %u)", Text, (uint32_t)ErrorValue);
        return {};
    }
    Data->freeIndices.Reserve(Data->assets.Size() + Plan.entries.Size());
    Data->retirements.Reserve(Data->assets.Size() + Plan.entries.Size());
    for (const AssetCatalogEntry* EntryPointer : Plan.entries)
    {
        Data->create(*EntryPointer);
    }
    const AssetHandle Handle = hmgetp_null(Data->pIndices, Id)->value;
    ++Data->assets[Handle.index].references;
    return Handle;
}

bool AssetManager::Release(AssetHandle Handle) { return Data->Release(Handle); }

void AssetManager::Update()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::Update");
    AssetManagerData& DataValue = *Data;
    DataValue.retire();
    // Only assets with work in flight are polled. A slot freed while still listed drops out here.
    uint32_t Kept = 0;
    for (uint32_t I = 0; I < DataValue.pending.Size(); ++I)
    {
        if (DataValue.Update(DataValue.assets[DataValue.pending[I]]))
        {
            DataValue.pending[Kept++] = DataValue.pending[I];
        }
    }
    DataValue.pending.Resize(Kept);
    DataValue.retire();
}

AssetState AssetManager::GetState(AssetHandle Handle) const
{
    const ManagedAsset* AssetPointer = Data->Find(Handle);
    return AssetPointer ? AssetPointer->state : AssetState::Invalid;
}

AssetError AssetManager::GetError(AssetHandle Handle) const
{
    const ManagedAsset* AssetPointer = Data->Find(Handle);
    return AssetPointer ? AssetPointer->error : AssetError::UnknownAsset;
}

const char* AssetManager::GetDiagnostic(AssetHandle Handle) const
{
    const ManagedAsset* AssetPointer = Data->Find(Handle);
    return AssetPointer ? AssetPointer->diagnostic : "";
}

const SceneAsset* AssetManager::GetScene(AssetHandle Handle) const { return (const SceneAsset*)Data->getPayload(Handle, AssetType::Scene); }

const TextureAsset* AssetManager::GetTexture(AssetHandle Handle) const
{
    return (const TextureAsset*)Data->getPayload(Handle, AssetType::Texture);
}
const MeshAsset*     AssetManager::GetMesh(AssetHandle Handle) const { return (const MeshAsset*)Data->getPayload(Handle, AssetType::Mesh); }
const MaterialAsset* AssetManager::GetMaterial(AssetHandle Handle) const
{
    return (const MaterialAsset*)Data->getPayload(Handle, AssetType::Material);
}
const SkeletonAsset* AssetManager::GetSkeleton(AssetHandle Handle) const
{
    return (const SkeletonAsset*)Data->getPayload(Handle, AssetType::Skeleton);
}
const AnimationClipAsset* AssetManager::GetAnimationClip(AssetHandle Handle) const
{
    return (const AnimationClipAsset*)Data->getPayload(Handle, AssetType::AnimationClip);
}

AssetHandle AssetManager::Find(AssetID Id) const
{
    const AssetIndex* IndexPointer = hmgetp_null(Data->pIndices, Id);
    return IndexPointer ? IndexPointer->value : AssetHandle{};
}

uint64_t AssetManager::GetStateRevision() const { return Data->tableRevision; }

GpuAssetTables AssetManager::GetGpuTables()
{
    AssetManagerData& DataValue = *Data;
    if (DataValue.builtRevision != DataValue.tableRevision)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AssetManager::encodeGpuTables");
        DataValue.encodeTables();
        DataValue.builtRevision = DataValue.tableRevision;
    }
    return { .meshes = { DataValue.meshes.GetData(), DataValue.meshes.Size() },
             .vertexStreams = { DataValue.vertexStreams.GetData(), DataValue.vertexStreams.Size() },
             .submeshes = { DataValue.submeshes.GetData(), DataValue.submeshes.Size() },
             .materials = { DataValue.materials.GetData(), DataValue.materials.Size() },
             .textures = { DataValue.textures.GetData(), DataValue.textures.Size() },
             .revision = DataValue.builtRevision };
}

uint32_t AssetManager::GetCount() const { return (uint32_t)hmlen(Data->pIndices); }

void AssetManager::Shutdown()
{
    if (Data->shuttingDown)
    {
        return;
    }
    Data->shuttingDown = true;
    Data->pending.Clear();
    for (ManagedAsset& AssetValue : Data->assets)
    {
        if (!AssetValue.pEntry)
        {
            continue;
        }
        if (AssetValue.pRequest)
        {
            const AssetLoader& Loader = Data->loaders[(uint32_t)AssetValue.pEntry->Type];
            Loader.pDestroyRequest(AssetValue.pRequest, Loader.UserData);
            AssetValue.pRequest = nullptr;
        }
        if (AssetValue.uploading)
        {
            const AssetLoader& Loader = Data->loaders[(uint32_t)AssetValue.pEntry->Type];
            Loader.pWaitUpload(AssetValue.pPayload, Loader.UserData);
            AssetValue.uploading = false;
        }
    }
    // Shutdown happens between frames, so idling covers every submission that could reference a payload.
    if (Data->pContext)
    {
        Data->pContext->WaitIdle();
    }
    Data->retirements.Clear();
    for (ManagedAsset& AssetValue : Data->assets)
    {
        if (!AssetValue.pEntry)
        {
            continue;
        }
        AssetValue.references = 0;
        AssetValue.state = AssetState::Retiring;
        AssetValue.gpuUse = 0;
        AssetValue.retirementQueued = false;
    }
    // Discard external references, retaining dependency edges so parents retire first.
    for (const ManagedAsset& AssetValue : Data->assets)
    {
        if (AssetValue.pDependencies)
        {
            for (uint32_t I = 0; I < AssetValue.pEntry->dependencies.Count; ++I)
            {
                ++Data->assets[AssetValue.pDependencies[I].index].references;
            }
        }
    }
    for (uint32_t I = 0; I < Data->assets.Size(); ++I)
    {
        if (Data->assets[I].pEntry && !Data->assets[I].references)
        {
            Data->queueRetirement(I);
        }
    }
    Data->retire();
    ASSERT(!GetCount());
}
} // namespace hz
