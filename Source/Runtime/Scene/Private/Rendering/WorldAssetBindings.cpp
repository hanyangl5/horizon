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
    WorldMeshBinding(Entity entity, AssetManager& assetManager, Array<AssetHandle>& releases):
        entity(entity), assetManager(assetManager), releases(releases)
    {
    }

    ~WorldMeshBinding()
    {
        for (const MaterialAssetBinding& material : materials)
            assetManager.release(material.handle);
        assetManager.release(view.mesh);
    }

    void sync(const MeshRenderer& renderer)
    {
        seen = true;
        bool changed = !initialized || meshID != renderer.mesh || overrides.Size() != renderer.overrides.Size();
        for (uint32_t i = 0; !changed && i < overrides.Size(); ++i)
            changed = overrides[i].submesh != renderer.overrides[i].submesh || overrides[i].material != renderer.overrides[i].material;
        if (!changed)
            return;

        view.state = AssetBindingState::Pending;
        view.materials = {};
        needsResolve = true;
        overrides.resize(renderer.overrides.Size());
        for (uint32_t i = 0; i < overrides.Size(); ++i)
            overrides[i] = renderer.overrides[i];
        if (!initialized || meshID != renderer.mesh)
        {
            const AssetHandle previous = view.mesh;
            view.mesh = {};
            meshID = renderer.mesh;
            if (!meshID.isValid() || !acquire(meshID, view.mesh))
                fail("Invalid or unavailable mesh asset");
            if (previous.isValid())
                releases.PushBack(previous);
        }
        initialized = true;
    }

    void update()
    {
        if (view.state == AssetBindingState::Failed)
            return;
        if (!view.mesh.isValid() && !acquire(meshID, view.mesh))
        {
            fail("Cannot acquire mesh asset");
            return;
        }
        const AssetBindingState meshState = state(view.mesh);
        if (meshState == AssetBindingState::Pending)
        {
            releaseMaterials();
            return;
        }
        const MeshAsset* pMesh = assetManager.getMesh(view.mesh);
        if (meshState == AssetBindingState::Failed || !pMesh)
        {
            fail("Mesh asset failed to load or has the wrong type");
            return;
        }
        if (needsResolve && !resolve(*pMesh))
        {
            fail("Invalid submesh overrides or unavailable material asset");
            return;
        }
        view.state = AssetBindingState::Ready;
        for (MaterialAssetBinding& material : materials)
        {
            if (!material.handle.isValid() && !acquire(material.asset, material.handle))
            {
                fail("Cannot acquire material asset");
                return;
            }
            const AssetBindingState materialState = state(material.handle);
            if (materialState == AssetBindingState::Failed ||
                (materialState == AssetBindingState::Ready && !assetManager.getMaterial(material.handle)))
            {
                fail("Material asset failed to load or has the wrong type");
                return;
            }
            if (materialState == AssetBindingState::Pending)
                view.state = AssetBindingState::Pending;
        }
    }

    Entity           entity;
    MeshAssetBinding view;
    bool             seen = false;

private:
    bool acquire(AssetID id, AssetHandle& handle)
    {
        if (assetManager.getState(assetManager.find(id)) == AssetState::Retiring)
        {
            return true;
        }
        AssetError error;
        handle = assetManager.request(id, &error);
        return handle.isValid() || error == AssetError::Retiring;
    }

    AssetBindingState state(AssetHandle handle) const
    {
        if (!handle.isValid())
            return AssetBindingState::Pending;
        switch (assetManager.getState(handle))
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
        for (const MaterialAssetBinding& material : materials)
            if (material.handle.isValid())
                releases.PushBack(material.handle);
        materials.Clear();
        view.materials = {};
    }

    void fail(const char* pMessage)
    {
        char text[kIDStringCapacity];
        meshID.toString(text);
        LOGF(Error, "World mesh '%s': %s", text, pMessage);
        releaseMaterials();
        view.state = AssetBindingState::Failed;
    }

    bool resolve(const MeshAsset& mesh)
    {
        Array<MaterialOverride> sorted({ overrides.GetData(), overrides.Size() });
        if (!sorted.Empty())
            qsort(sorted.GetData(), sorted.Size(), sizeof(MaterialOverride), compareMaterialOverrides);
        for (uint32_t i = 0; i < sorted.Size(); ++i)
            if (!sorted[i].submesh || !sorted[i].material.isValid() || (i && sorted[i - 1].submesh == sorted[i].submesh))
                return false;

        Array<MaterialAssetBinding>   next;
        const Span<SceneAssetSubmesh> submeshes = mesh.getDescription().submeshes;
        next.Reserve(submeshes.Count);
        uint32_t matched = 0;
        for (const SceneAssetSubmesh& submesh : submeshes)
        {
            uint32_t first = 0, end = sorted.Size();
            while (first < end)
            {
                const uint32_t middle = first + (end - first) / 2;
                if (sorted[middle].submesh < submesh.id)
                    first = middle + 1;
                else
                    end = middle;
            }
            const bool overridden = first < sorted.Size() && sorted[first].submesh == submesh.id;
            matched += overridden;
            next.PushBack({ .submesh = submesh.id, .asset = overridden ? sorted[first].material : submesh.material });
        }
        if (matched != sorted.Size())
            return false;
        // Acquire replacements before releasing old leases, including handoffs between entities in this update.
        bool acquired = true;
        for (MaterialAssetBinding& material : next)
            acquired &= acquire(material.asset, material.handle);
        releaseMaterials();
        materials = std::move(next);
        view.materials = { materials.GetData(), materials.Size() };
        needsResolve = false;
        return acquired;
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
    WorldAssetBindingsData(World& world, AssetManager& assetManager):
        world(world), assetManager(assetManager), query(world, { SceneTypes::meshRenderer })
    {
        ASSERT(query.isValid());
    }
    ~WorldAssetBindingsData()
    {
        Clear();
        hmfree(pIndices);
    }

    void Clear()
    {
        for (WorldMeshBinding* pBinding : bindings)
        {
            hmdel(pIndices, pBinding->entity);
            MemDelete(pBinding);
        }
        bindings.Clear();
    }

    static bool sync(Entity entity, Span<const void*> fields, void* User)
    {
        WorldAssetBindingsData&  data = *(WorldAssetBindingsData*)User;
        const WorldBindingIndex* pIndex = hmgetp_null(data.pIndices, entity);
        const uint32_t           index = pIndex ? pIndex->value : data.bindings.Size();
        if (!pIndex)
        {
            data.bindings.PushBack(MemNew(WorldMeshBinding, entity, data.assetManager, data.releases));
            const WorldBindingIndex entry = { .key = entity, .value = index };
            hmputs(data.pIndices, entry);
        }
        data.bindings[index]->sync(*(const MeshRenderer*)fields.Data[0]);
        return true;
    }

    void update()
    {
        ASSERT(!world.isDeferred());
        for (WorldMeshBinding* pBinding : bindings)
            pBinding->seen = false;
        query.each(sync, this);
        for (WorldMeshBinding* pBinding : bindings)
            if (pBinding->seen)
                pBinding->update();
        for (uint32_t i = 0; i < bindings.Size();)
        {
            WorldMeshBinding* pBinding = bindings[i];
            if (pBinding->seen)
            {
                ++i;
                continue;
            }
            hmdel(pIndices, pBinding->entity);
            MemDelete(pBinding);
            bindings[i] = bindings[bindings.Size() - 1];
            bindings.PopBack();
            if (i < bindings.Size())
                hmgetp_null(pIndices, bindings[i]->entity)->value = i;
        }
        for (AssetHandle handle : releases)
            assetManager.release(handle);
        releases.Clear();
    }

    World&                   world;
    AssetManager&            assetManager;
    WorldQuery               query;
    Array<WorldMeshBinding*> bindings;
    Array<AssetHandle>       releases;
    WorldBindingIndex*       pIndices = nullptr;
};

WorldAssetBindings::WorldAssetBindings(World& world, AssetManager& assetManager): Data(MemNew(WorldAssetBindingsData, world, assetManager))
{
}
WorldAssetBindings::~WorldAssetBindings() { MemDelete(Data); }
void                    WorldAssetBindings::update() { Data->update(); }
void                    WorldAssetBindings::Clear() { Data->Clear(); }
uint32_t                WorldAssetBindings::getCount() const { return Data->bindings.Size(); }
const MeshAssetBinding* WorldAssetBindings::get(Entity entity) const
{
    const WorldBindingIndex* pIndex = hmgetp_null(Data->pIndices, entity);
    return pIndex ? &Data->bindings[pIndex->value]->view : nullptr;
}
} // namespace hz
