/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Scene/AssetManager.h"

namespace hz
{
class SceneAssetLoaderData;

class SceneAssetLoader
{
public:
    explicit SceneAssetLoader(ResourceDirectory cookedDirectory = DirectoryMeshes);
    ~SceneAssetLoader();
    SceneAssetLoader(const SceneAssetLoader&) = delete;
    SceneAssetLoader& operator=(const SceneAssetLoader&) = delete;

    AssetLoader GetLoader();

private:
    SceneAssetLoaderData* Data = nullptr;
};
} // namespace hz
