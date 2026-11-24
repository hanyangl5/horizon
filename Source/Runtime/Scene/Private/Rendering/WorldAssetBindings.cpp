/* Copyright (c) 2026 Horizon */

#include "Scene/WorldAssetBindings.h"
#include "Scene/MeshAsset.h"
#include "Core/IContainer.h"
#include "../SceneMaterialOverrides.h"

#include <stdlib.h>

#include "Core/IMemory.h"

namespace hz
{
class WorldMeshBinding
{
public:
    WorldMeshBinding(Entity EntityHandle, AssetManager& AssetManagerValue, Array<AssetHandle>& Releases):
        entity(EntityHandle), assetManager(AssetManagerValue), releases(Releases)
    {
    }

    ~WorldMeshBinding()
    {
        for (const MaterialAssetBinding& MaterialValue : materials)
        {
            assetManager.Release(MaterialValue.Handle);
        }
        assetManager.Release(view.mesh);
    }

    void sync(const MeshRenderer& RendererValue)
    {
        seen = true;
        bool Changed = !initialized || meshID != RendererValue.mesh || overrides.Size() != RendererValue.overrides.Size();
        for (uint32_t I = 0; !Changed && I < overrides.Size(); ++I)
        {
            Changed =
                overrides[I].submesh != RendererValue.overrides[I].submesh || overrides[I].material != RendererValue.overrides[I].material;
        }
        if (!Changed)
        {
            return;
        }

        view.state = AssetBindingState::Pending;
        view.materials = {};
        needsResolve = true;
        overrides.Resize(RendererValue.overrides.Size());
        for (uint32_t I = 0; I < overrides.Size(); ++I)
        {
            overrides[I] = RendererValue.overrides[I];
        }
        if (!initialized || meshID != RendererValue.mesh)
        {
            const AssetHandle Previous = view.mesh;
            view.mesh = {};
            meshID = RendererValue.mesh;
            if (!meshID.IsValid() || !acquire(meshID, view.mesh))
            {
                fail("Invalid or unavailable mesh asset");
            }
            if (Previous.IsValid())
            {
                releases.PushBack(Previous);
            }
        }
        initialized = true;
    }

    void Update()
    {
        if (view.state == AssetBindingState::Failed)
        {
            return;
        }
        if (!view.mesh.IsValid() && !acquire(meshID, view.mesh))
        {
            fail("Cannot acquire mesh asset");
            return;
        }
        const AssetBindingState MeshState = state(view.mesh);
        if (MeshState == AssetBindingState::Pending)
        {
            releaseMaterials();
            return;
        }
        const MeshAsset* MeshPointer = assetManager.GetMesh(view.mesh);
        if (MeshState == AssetBindingState::Failed || !MeshPointer)
        {
            fail("Mesh asset failed to Load or has the wrong type");
            return;
        }
        if (needsResolve && !resolve(*MeshPointer))
        {
            fail("Invalid submesh overrides or unavailable material asset");
            return;
        }
        view.state = AssetBindingState::Ready;
        for (MaterialAssetBinding& MaterialValue : materials)
        {
            if (!MaterialValue.Handle.IsValid() && !acquire(MaterialValue.asset, MaterialValue.Handle))
            {
                fail("Cannot acquire material asset");
                return;
            }
            const AssetBindingState MaterialState = state(MaterialValue.Handle);
            if (MaterialState == AssetBindingState::Failed ||
                (MaterialState == AssetBindingState::Ready && !assetManager.GetMaterial(MaterialValue.Handle)))
            {
                fail("Material asset failed to Load or has the wrong type");
                return;
            }
            if (MaterialState == AssetBindingState::Pending)
            {
                view.state = AssetBindingState::Pending;
            }
        }
    }

    // Pending bindings, including ones a MeshRenderer edit just reset, still need Update to acquire and resolve assets.
    bool IsWaiting() const { return view.state == AssetBindingState::Pending; }

    Entity           entity;
    MeshAssetBinding view;
    bool             seen = false;

private:
    bool acquire(AssetID Id, AssetHandle& Handle)
    {
        if (assetManager.GetState(assetManager.Find(Id)) == AssetState::Retiring)
        {
            return true;
        }
        AssetError ErrorValue;
        Handle = assetManager.Request(Id, &ErrorValue);
        return Handle.IsValid() || ErrorValue == AssetError::Retiring;
    }

    AssetBindingState state(AssetHandle Handle) const
    {
        if (!Handle.IsValid())
        {
            return AssetBindingState::Pending;
        }
        switch (assetManager.GetState(Handle))
        {
        case AssetState::Ready:
            return AssetBindingState::Ready;
        case AssetState::Invalid:
        case AssetState::Failed:
        case AssetState::Retiring:
            return AssetBindingState::Failed;
        default:
            return AssetBindingState::Pending;
        }
    }

    void releaseMaterials()
    {
        for (const MaterialAssetBinding& MaterialValue : materials)
        {
            if (MaterialValue.Handle.IsValid())
            {
                releases.PushBack(MaterialValue.Handle);
            }
        }
        materials.Clear();
        view.materials = {};
    }

    void fail(const char* Message)
    {
        char Text[kIDStringCapacity];
        meshID.ToString(Text);
        LOGF(Error, "World mesh '%s': %s", Text, Message);
        releaseMaterials();
        view.state = AssetBindingState::Failed;
    }

    bool resolve(const MeshAsset& MeshValue)
    {
        Array<MaterialOverride> Sorted({ overrides.GetData(), overrides.Size() });
        if (!Sorted.Empty())
        {
            qsort(Sorted.GetData(), Sorted.Size(), sizeof(MaterialOverride), CompareMaterialOverrides);
        }
        for (uint32_t I = 0; I < Sorted.Size(); ++I)
        {
            if (!Sorted[I].submesh || !Sorted[I].material.IsValid() || (I && Sorted[I - 1].submesh == Sorted[I].submesh))
            {
                return false;
            }
        }

        Array<MaterialAssetBinding>   Next;
        const Span<SceneAssetSubmesh> Submeshes = MeshValue.GetDescription().submeshes;
        Next.Reserve(Submeshes.Count);
        uint32_t Matched = 0;
        for (const SceneAssetSubmesh& Submesh : Submeshes)
        {
            uint32_t First = 0, End = Sorted.Size();
            while (First < End)
            {
                const uint32_t Middle = First + (End - First) / 2;
                if (Sorted[Middle].submesh < Submesh.id)
                {
                    First = Middle + 1;
                }
                else
                {
                    End = Middle;
                }
            }
            const bool Overridden = First < Sorted.Size() && Sorted[First].submesh == Submesh.id;
            Matched += Overridden;
            Next.PushBack({ .submesh = Submesh.id, .asset = Overridden ? Sorted[First].material : Submesh.material });
        }
        if (Matched != Sorted.Size())
        {
            return false;
        }
        // Acquire replacements before releasing old leases, including handoffs between entities in this Update.
        bool Acquired = true;
        for (MaterialAssetBinding& MaterialValue : Next)
        {
            Acquired &= acquire(MaterialValue.asset, MaterialValue.Handle);
        }
        releaseMaterials();
        materials = std::move(Next);
        view.materials = { materials.GetData(), materials.Size() };
        needsResolve = false;
        return Acquired;
    }

    AssetManager&               assetManager;
    Array<AssetHandle>&         releases;
    AssetID                     meshID;
    Array<MaterialOverride>     overrides;
    Array<MaterialAssetBinding> materials;
    bool                        initialized = false;
    bool                        needsResolve = true;
};

struct WorldBindingIndex
{
    Entity   key;
    uint32_t value;
};

class WorldAssetBindingsData
{
public:
    WorldAssetBindingsData(World& WorldData, AssetManager& AssetManagerValue):
        world(WorldData), assetManager(AssetManagerValue), query(WorldData, { SceneTypes::meshRenderer }), changes(WorldData)
    {
        ASSERT(query.IsValid());
    }
    ~WorldAssetBindingsData()
    {
        Clear();
        hmfree(pIndices);
    }

    void Clear()
    {
        for (WorldMeshBinding* Binding : bindings)
        {
            hmdel(pIndices, Binding->entity);
            MemDelete(Binding);
        }
        bindings.Clear();
        rescanPending = true;
    }

    static bool sync(Entity EntityHandle, Span<const void*> Fields, void* User)
    {
        WorldAssetBindingsData&  DataValue = *(WorldAssetBindingsData*)User;
        const WorldBindingIndex* IndexPointer = hmgetp_null(DataValue.pIndices, EntityHandle);
        const uint32_t           IndexValue = IndexPointer ? IndexPointer->value : DataValue.bindings.Size();
        if (!IndexPointer)
        {
            DataValue.bindings.PushBack(MemNew(WorldMeshBinding, EntityHandle, DataValue.assetManager, DataValue.releases));
            const WorldBindingIndex Entry = { .key = EntityHandle, .value = IndexValue };
            hmputs(DataValue.pIndices, Entry);
        }
        DataValue.bindings[IndexValue]->sync(*(const MeshRenderer*)Fields.Data[0]);
        return true;
    }

    void remove(uint32_t IndexValue)
    {
        WorldMeshBinding* Binding = bindings[IndexValue];
        hmdel(pIndices, Binding->entity);
        MemDelete(Binding);
        bindings[IndexValue] = bindings[bindings.Size() - 1];
        bindings.PopBack();
        if (IndexValue < bindings.Size())
        {
            hmgetp_null(pIndices, bindings[IndexValue]->entity)->value = IndexValue;
        }
    }

    // Syncs one entity named by the change log; returns false when it no longer has a MeshRenderer.
    void syncChanged(Entity EntityHandle)
    {
        const WorldBindingIndex* IndexPointer = hmgetp_null(pIndices, EntityHandle);
        const MeshRenderer*      RendererPointer = (const MeshRenderer*)world.GetComponent(EntityHandle, SceneTypes::meshRenderer);
        if (!RendererPointer)
        {
            if (IndexPointer)
            {
                remove(IndexPointer->value);
            }
            return;
        }
        const void* Field = RendererPointer;
        sync(EntityHandle, { &Field, 1 }, this);
    }

    void Update()
    {
        ASSERT(!world.IsDeferred());
        // Bindings depend only on their MeshRenderer and on asset states, so a frame without component changes and without
        // asset state changes only revisits bindings that are still waiting for an asset.
        bool               Rescan = false;
        const Span<Entity> Changed = changes.Drain(Rescan);
        const uint64_t     AssetRevision = assetManager.GetStateRevision();
        Rescan |= rescanPending;
        rescanPending = false;
        if (Rescan)
        {
            for (WorldMeshBinding* Binding : bindings)
            {
                Binding->seen = false;
            }
            query.Each(sync, this);
            for (uint32_t I = 0; I < bindings.Size();)
            {
                if (bindings[I]->seen)
                {
                    ++I;
                }
                else
                {
                    remove(I);
                }
            }
        }
        else
        {
            for (Entity EntityHandle : Changed)
            {
                syncChanged(EntityHandle);
            }
        }
        const bool AssetsChanged = Rescan || AssetRevision != syncedAssetRevision;
        for (WorldMeshBinding* Binding : bindings)
        {
            if (AssetsChanged || Binding->IsWaiting())
            {
                Binding->Update();
            }
        }
        syncedAssetRevision = AssetRevision;
        for (AssetHandle Handle : releases)
        {
            assetManager.Release(Handle);
        }
        releases.Clear();
    }

    World&                   world;
    AssetManager&            assetManager;
    WorldQuery               query;
    WorldChangeLog           changes;
    Array<WorldMeshBinding*> bindings;
    Array<AssetHandle>       releases;
    WorldBindingIndex*       pIndices = nullptr;
    uint64_t                 syncedAssetRevision = 0;
    bool                     rescanPending = false;
};

WorldAssetBindings::WorldAssetBindings(World& WorldData, AssetManager& AssetManagerValue):
    Data(MemNew(WorldAssetBindingsData, WorldData, AssetManagerValue))
{
}
WorldAssetBindings::~WorldAssetBindings() { MemDelete(Data); }
void                    WorldAssetBindings::Update() { Data->Update(); }
void                    WorldAssetBindings::Clear() { Data->Clear(); }
uint32_t                WorldAssetBindings::GetCount() const { return Data->bindings.Size(); }
const MeshAssetBinding* WorldAssetBindings::Get(Entity EntityHandle) const
{
    const WorldBindingIndex* IndexPointer = hmgetp_null(Data->pIndices, EntityHandle);
    return IndexPointer ? &Data->bindings[IndexPointer->value]->view : nullptr;
}
} // namespace hz
