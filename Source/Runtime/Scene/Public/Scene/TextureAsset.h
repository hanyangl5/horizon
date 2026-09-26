/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Scene/AssetRegistry.h"

namespace hz
{
class GPUTexture;
class TextureAssetData;
class TextureAssetLoaderData;

class TextureAsset
{
public:
    TextureAsset() = default;
    ~TextureAsset();
    TextureAsset(const TextureAsset&) = delete;
    TextureAsset& operator=(const TextureAsset&) = delete;

    const GPUTexture& getTexture() const;

private:
    TextureAssetData* pData = nullptr;
    friend class TextureAssetLoaderData;
};

class TextureAssetLoader
{
public:
    explicit TextureAssetLoader(RenderContext& context, ResourceDirectory cookedDirectory = RD_MESHES,
                                ResourceDirectory sourceDirectory = RD_TEXTURES);
    ~TextureAssetLoader();
    TextureAssetLoader(const TextureAssetLoader&) = delete;
    TextureAssetLoader& operator=(const TextureAssetLoader&) = delete;

    AssetLoader getLoader();

private:
    TextureAssetLoaderData* pData = nullptr;
};
} // namespace hz
