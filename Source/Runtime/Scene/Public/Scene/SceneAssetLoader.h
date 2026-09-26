/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Scene/AssetRegistry.h"

namespace hz
{
class SceneAssetLoaderData;

class SceneAssetLoader
{
public:
    explicit SceneAssetLoader(ResourceDirectory cookedDirectory = RD_MESHES);
    ~SceneAssetLoader();
    SceneAssetLoader(const SceneAssetLoader&) = delete;
    SceneAssetLoader& operator=(const SceneAssetLoader&) = delete;

    AssetLoader getLoader();

private:
    SceneAssetLoaderData* pData = nullptr;
};
} // namespace hz
