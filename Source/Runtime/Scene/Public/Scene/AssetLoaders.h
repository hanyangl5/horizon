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
    ResourceDirectory cookedDirectory = DirectoryMeshes;
    ResourceDirectory sourceDirectory = DirectoryTextures;
};

// Owns the typed loaders and shared decoded documents/geometry. Must outlive registered asset managers.
class AssetLoaders
{
public:
    AssetLoaders(RenderContext& context, const AssetLoadersDesc& desc);
    ~AssetLoaders();
    AssetLoaders(const AssetLoaders&) = delete;
    AssetLoaders& operator=(const AssetLoaders&) = delete;

    void RegisterLoaders(AssetManager& assetManager);

private:
    AssetLoadersData* Data = nullptr;
};
} // namespace hz
