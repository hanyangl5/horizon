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
    AssetType     type = AssetType::Scene;
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
    bool                     addScene(const SceneAsset& scene, const char* pCookedScenePath);
    bool                     add(const AssetCatalogEntry& entry);
    bool                     merge(const AssetCatalog& other);
    bool                     replace(const AssetCatalogEntry& entry);
    bool                     remove(AssetID id);
    // Atomic full replacement from the current-format index. Failed reads leave the catalog unchanged.
    bool                     parse(Span<char> json);
    bool                     load(ResourceDirectory directory, const char* pFileName);
    bool                     serialize(Array<char>& json) const;
    // Entries own their metadata and stay at stable addresses until clear or owner destruction.
    const AssetCatalogEntry* find(AssetID id) const;
    // Bootstrap a Scene AssetID from its relative cooked document path.
    const AssetCatalogEntry* findScene(const char* pCookedScenePath) const;
    uint32_t                 getCount() const;
    void                     clear();

private:
    bool              set(const AssetCatalogEntry& entry, bool replaceExisting);
    AssetCatalogData* pData = nullptr;
};
} // namespace hz
