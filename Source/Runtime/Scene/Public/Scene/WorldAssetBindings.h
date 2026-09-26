/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/AssetRegistry.h"
#include "Scene/World.h"

namespace hz
{
enum class AssetBindingState : uint32_t
{
    Pending,
    Ready,
    Failed,
};

struct MaterialAssetBinding
{
    uint64_t    submesh = 0;
    AssetID     asset;
    AssetHandle handle;
};

struct MeshAssetBinding
{
    AssetHandle                mesh;
    // Same order as MeshAsset::getDescription().submeshes; handles are borrowed from this binding.
    Span<MaterialAssetBinding> materials;
    AssetBindingState          state = AssetBindingState::Pending;
};

class WorldAssetBindingsData;

class WorldAssetBindings
{
public:
    // World and registry must outlive bindings. Destroy bindings before registry shutdown or World replacement.
    WorldAssetBindings(World& world, AssetRegistry& registry);
    ~WorldAssetBindings();
    WorldAssetBindings(const WorldAssetBindings&) = delete;
    WorldAssetBindings& operator=(const WorldAssetBindings&) = delete;

    // Call outside World query/defer scopes. The caller advances registry loading with AssetRegistry::update().
    void                    update();
    // Views expire at the next update/clear. Pending and failed bindings must not be rendered.
    const MeshAssetBinding* get(Entity entity) const;
    uint32_t                getCount() const;
    // Releases all leases; the next update rebuilds bindings, including previously failed requests.
    void                    clear();

private:
    WorldAssetBindingsData* pData = nullptr;
};
} // namespace hz
