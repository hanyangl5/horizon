/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ISpan.h"
#include "Scene/SceneComponents.h"
#include "Scene/World.h"

namespace hz
{
struct SceneAssetTexture
{
    AssetID     id;
    const char* pPath = "";
    const char* pSource = "";
    bool        srgb = false;
    bool        cooked = false; // Path is relative to the cooked output root; otherwise the source root.
};

enum class MaterialAlphaMode : uint32_t
{
    Opaque,
    Mask,
    Blend,
};

struct SceneAssetMaterial
{
    AssetID           id;
    const char*       pName = "";
    AssetID           baseColorTexture;
    AssetID           normalTexture;
    AssetID           metallicRoughnessTexture;
    AssetID           emissiveTexture;
    float             baseColorFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float             metallicFactor = 1.0f;
    float             roughnessFactor = 1.0f;
    float             emissiveFactor[3] = {};
    MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;
    float             alphaCutoff = 0.5f;
    bool              doubleSided = false;
};

struct SceneAssetSubmesh
{
    SubmeshID id = 0;
    uint32_t  draw = 0;
    AssetID   material;
};

struct SceneAssetMesh
{
    AssetID                 id;
    const char*             pName = "";
    const char*             pGeometry = "";
    Span<SceneAssetSubmesh> submeshes;
};

struct SceneAssetNode
{
    const char*    pName = "";
    uint32_t       parent = UINT32_MAX;
    AssetID        mesh;
    LocalTransform transform;
    LocalMatrix    matrix;
    // Only the selected representation is used; matrices are never decomposed.
    bool           hasMatrix = false;
};

class SceneAssetData;

class SceneAsset
{
public:
    SceneAsset() = default;
    ~SceneAsset();
    SceneAsset(SceneAsset&& other) noexcept;
    SceneAsset& operator=(SceneAsset&& other) noexcept;
    SceneAsset(const SceneAsset&) = delete;
    SceneAsset& operator=(const SceneAsset&) = delete;

    // Owns decoded data. Failed reads preserve the previously loaded asset.
    bool                     parse(Span<char> json);
    bool                     isValid() const { return pData != nullptr; }
    AssetID                  getID() const;
    const char*              getSource() const;
    const char*              getContentHash() const;
    // Views and strings remain valid until the owning payload is replaced or destroyed.
    Span<SceneAssetNode>     getNodes() const;
    Span<SceneAssetMesh>     getMeshes() const;
    Span<SceneAssetMaterial> getMaterials() const;
    Span<SceneAssetTexture>  getTextures() const;
    // Creates independent authoring objects beneath a new root, owned by World.
    // Requires builtin types and a committed World; the payload can be released afterwards.
    // ObjectID generation failure returns an invalid Entity and rolls back the new objects.
    Entity                   instantiate(World& world, Entity parent = {}) const;

private:
    SceneAssetData* pData = nullptr;
};
} // namespace hz
