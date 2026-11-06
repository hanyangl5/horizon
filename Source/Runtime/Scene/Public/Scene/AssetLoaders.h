/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/AssetManager.h"
#include "RHI/IGraphics.h"

namespace hz
{
class AssetLoadersData;

struct AssetLoadersDesc
{
    VertexLayout      vertexLayout = {};
    ResourceDirectory cookedDirectory = RD_MESHES;
    ResourceDirectory sourceDirectory = RD_TEXTURES;
};

// Owns the typed loaders and shared decoded documents/geometry. Must outlive registered asset managers.
class AssetLoaders
{
public:
    AssetLoaders(RenderContext& context, const AssetLoadersDesc& desc);
    ~AssetLoaders();
    AssetLoaders(const AssetLoaders&) = delete;
    AssetLoaders& operator=(const AssetLoaders&) = delete;

    void registerLoaders(AssetManager& assetManager);

private:
    AssetLoadersData* pData = nullptr;
};
} // namespace hz
