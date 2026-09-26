/* Copyright (c) 2026 Horizon */

#include "Scene/WorldAssetBindings.h"
#include "Scene/MeshAsset.h"
#include "Core/IContainer.h"

#include <stdlib.h>

#include "Core/IMemory.h"

namespace hz
{
static int compareOverrides(const void* pA, const void* pB)
{
    const SubmeshID a = ((const MaterialOverride*)pA)->submesh;
    const SubmeshID b = ((const MaterialOverride*)pB)->submesh;
    return (a > b) - (a < b);
}

class WorldMeshBinding
{
public:
    WorldMeshBinding(Entity entity, AssetRegistry& registry, Array<AssetHandle>& releases):
        entity(entity), registry(registry), releases(releases)
    {
    }

    ~WorldMeshBinding()
    {
        for (const MaterialAssetBinding& material : materials)
            registry.release(material.handle);
        registry.release(view.mesh);
    }

    void sync(const MeshRenderer& renderer)
    {
        seen = true;
        bool changed = !initialized || meshID != renderer.mesh || overrides.size() != renderer.overrides.size();
        for (uint32_t i = 0; !changed && i < overrides.size(); ++i)
            changed = overrides[i].submesh != renderer.overrides[i].submesh || overrides[i].material != renderer.overrides[i].material;
        if (!changed)
            return;

        view.state = AssetBindingState::Pending;
        view.materials = {};
        needsResolve = true;
        overrides.resize(renderer.overrides.size());
        for (uint32_t i = 0; i < overrides.size(); ++i)
            overrides[i] = renderer.overrides[i];
        if (!initialized || meshID != renderer.mesh)
        {
            const AssetHandle previous = view.mesh;
            view.mesh = {};
            meshID = renderer.mesh;
            if (!meshID.isValid() || !acquire(meshID, view.mesh))
                fail("Invalid or unavailable mesh asset");
            if (previous.isValid())
                releases.pushBack(previous);
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
        const MeshAsset* pMesh = registry.getMesh(view.mesh);
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
                (materialState == AssetBindingState::Ready && !registry.getMaterial(material.handle)))
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
        if (registry.getState(registry.find(id)) == AssetState::Retiring)
            return true;
        AssetError error;
        handle = registry.request(id, &error);
        return handle.isValid() || error == AssetError::Retiring;
    }

    AssetBindingState state(AssetHandle handle) const
    {
        if (!handle.isValid())
            return AssetBindingState::Pending;
        switch (registry.getState(handle))
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
                releases.pushBack(material.handle);
        materials.clear();
        view.materials = {};
    }

    void fail(const char* pMessage)
    {
        char text[kIDStringCapacity];
        meshID.toString(text);
        LOGF(eERROR, "World mesh '%s': %s", text, pMessage);
        releaseMaterials();
        view.state = AssetBindingState::Failed;
    }

    bool resolve(const MeshAsset& mesh)
    {
        Array<MaterialOverride> sorted({ overrides.data(), overrides.size() });
        if (!sorted.empty())
            qsort(sorted.data(), sorted.size(), sizeof(MaterialOverride), compareOverrides);
        for (uint32_t i = 0; i < sorted.size(); ++i)
            if (!sorted[i].submesh || !sorted[i].material.isValid() || (i && sorted[i - 1].submesh == sorted[i].submesh))
                return false;

        Array<MaterialAssetBinding>   next;
        const Span<SceneAssetSubmesh> submeshes = mesh.getDescription().submeshes;
        next.reserve(submeshes.count);
        uint32_t matched = 0;
        for (const SceneAssetSubmesh& submesh : submeshes)
        {
            uint32_t first = 0, end = sorted.size();
            while (first < end)
            {
                const uint32_t middle = first + (end - first) / 2;
                if (sorted[middle].submesh < submesh.id)
                    first = middle + 1;
                else
                    end = middle;
            }
            const bool overridden = first < sorted.size() && sorted[first].submesh == submesh.id;
            matched += overridden;
            next.pushBack({ .submesh = submesh.id, .asset = overridden ? sorted[first].material : submesh.material });
        }
        if (matched != sorted.size())
            return false;
        // Acquire replacements before releasing old leases, including handoffs between entities in this update.
        bool acquired = true;
        for (MaterialAssetBinding& material : next)
            acquired &= acquire(material.asset, material.handle);
        releaseMaterials();
        materials = std::move(next);
        view.materials = { materials.data(), materials.size() };
        needsResolve = false;
        return acquired;
    }

    AssetRegistry&              registry;
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
    WorldAssetBindingsData(World& world, AssetRegistry& registry):
        world(world), registry(registry), query(world, { SceneTypes::meshRenderer })
    {
        ASSERT(query.isValid());
    }
    ~WorldAssetBindingsData()
    {
        clear();
        hmfree(pIndices);
    }

    void clear()
    {
        for (WorldMeshBinding* pBinding : bindings)
        {
            hmdel(pIndices, pBinding->entity);
            tf_delete(pBinding);
        }
        bindings.clear();
    }

    static bool sync(Entity entity, Span<const void*> fields, void* pUser)
    {
        WorldAssetBindingsData&  data = *(WorldAssetBindingsData*)pUser;
        const WorldBindingIndex* pIndex = hmgetp_null(data.pIndices, entity);
        const uint32_t           index = pIndex ? pIndex->value : data.bindings.size();
        if (!pIndex)
        {
            data.bindings.pushBack(tf_new(WorldMeshBinding, entity, data.registry, data.releases));
            const WorldBindingIndex entry = { .key = entity, .value = index };
            hmputs(data.pIndices, entry);
        }
        data.bindings[index]->sync(*(const MeshRenderer*)fields.pData[0]);
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
        for (uint32_t i = 0; i < bindings.size();)
        {
            WorldMeshBinding* pBinding = bindings[i];
            if (pBinding->seen)
            {
                ++i;
                continue;
            }
            hmdel(pIndices, pBinding->entity);
            tf_delete(pBinding);
            bindings[i] = bindings[bindings.size() - 1];
            bindings.popBack();
            if (i < bindings.size())
                hmgetp_null(pIndices, bindings[i]->entity)->value = i;
        }
        for (AssetHandle handle : releases)
            registry.release(handle);
        releases.clear();
    }

    World&                   world;
    AssetRegistry&           registry;
    WorldQuery               query;
    Array<WorldMeshBinding*> bindings;
    Array<AssetHandle>       releases;
    WorldBindingIndex*       pIndices = nullptr;
};

WorldAssetBindings::WorldAssetBindings(World& world, AssetRegistry& registry): pData(tf_new(WorldAssetBindingsData, world, registry)) {}
WorldAssetBindings::~WorldAssetBindings() { tf_delete(pData); }
void                    WorldAssetBindings::update() { pData->update(); }
void                    WorldAssetBindings::clear() { pData->clear(); }
uint32_t                WorldAssetBindings::getCount() const { return pData->bindings.size(); }
const MeshAssetBinding* WorldAssetBindings::get(Entity entity) const
{
    const WorldBindingIndex* pIndex = hmgetp_null(pData->pIndices, entity);
    return pIndex ? &pData->bindings[pIndex->value]->view : nullptr;
}
} // namespace hz
