/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"

struct cgltf_data;
struct cJSON;

class SceneAssetCooker
{
public:
    SceneAssetCooker(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory,
                     const char* pGeometryFile);
    ~SceneAssetCooker();
    SceneAssetCooker(const SceneAssetCooker&) = delete;
    SceneAssetCooker& operator=(const SceneAssetCooker&) = delete;

    bool isCurrent(const char* pContentHash) const;
    bool prepare();
    bool build(const cgltf_data& data, const cJSON& legacyManifest);
    bool write() const;

private:
    ResourceDirectory sourceDirectory;
    ResourceDirectory outputDirectory;
    char              metadataFile[FS_MAX_PATH] = {};
    char              assetFile[FS_MAX_PATH] = {};
    const char*       pSourceFile;
    const char*       pGeometryFile;
    cJSON*            pMetadata = nullptr;
    cJSON*            pPrevious = nullptr;
    cJSON*            pAsset = nullptr;
};
