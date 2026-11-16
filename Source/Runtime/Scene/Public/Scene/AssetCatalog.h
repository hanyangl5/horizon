/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ISpan.h"
#include "Core/IFileSystem.h"
#include "Scene/SceneID.h"

namespace hz
{
enum class AssetType : uint32_t
{
    Scene,
    Mesh,
    Material,
    Texture,
};

enum class AssetPathRoot : uint32_t
{
    Source,
    Cooked,
};

struct AssetLocation
{
    const char*   pPath = "";
    AssetPathRoot root = AssetPathRoot::Cooked;
    // Mesh/Material index in the SceneAsset table; UINT32_MAX denotes a standalone file.
    uint32_t      elementIndex = UINT32_MAX;
};

struct AssetCatalogEntry
{
    AssetID       id;
    AssetType     Type = AssetType::Scene;
    const char*   pSourcePath = "";
    AssetLocation location;
    Span<AssetID> dependencies;
    bool          srgb = false;
};

class SceneAsset;
class AssetCatalogData;
template<typename T>
class Array;

class AssetCatalog
{
public:
    AssetCatalog() = default;
    ~AssetCatalog();
    AssetCatalog(AssetCatalog&& other) noexcept;
    AssetCatalog& operator=(AssetCatalog&& other) noexcept;
    AssetCatalog(const AssetCatalog&) = delete;
    AssetCatalog& operator=(const AssetCatalog&) = delete;

    // Paths are relative to their source/cooked roots. Conflicts leave the catalog unchanged.
    bool                     AddScene(const SceneAsset& scene, const char* pCookedScenePath);
    bool                     Add(const AssetCatalogEntry& entry);
    bool                     Merge(const AssetCatalog& other);
    bool                     Replace(const AssetCatalogEntry& entry);
    bool                     Remove(AssetID id);
    // Atomic full replacement from the current-Format index. Failed reads leave the catalog unchanged.
    bool                     Parse(Span<char> json);
    bool                     Load(ResourceDirectory directory, const char* pFileName);
    bool                     Serialize(Array<char>& json) const;
    // Entries own their metadata and stay at stable addresses until Clear or owner destruction.
    const AssetCatalogEntry* Find(AssetID id) const;
    // Bootstrap a Scene AssetID from its relative cooked document path.
    const AssetCatalogEntry* FindScene(const char* pCookedScenePath) const;
    uint32_t                 GetCount() const;
    void                     Clear();

private:
    bool              Set(const AssetCatalogEntry& entry, bool replaceExisting);
    AssetCatalogData* Data = nullptr;
};
} // namespace hz
