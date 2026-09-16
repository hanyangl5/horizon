/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Core/IContainer.h"
#include "SceneTextureCooker.h"

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
    bool                      prepare(const cgltf_data& data);
    const CookedSceneTexture& getTexture(uint32_t index) const { return textures[index]; }
    bool                      build(const cgltf_data& data, const char* pContentHash);
    bool write() const;

private:
    cJSON*            createMaterial(const cgltf_data& data, uint32_t index, hz::Span<cJSON*> textureRecords, bool signature) const;
    bool              validate() const;
    bool              prepareTextures(const cgltf_data& data);
    bool              registerTexture(CookedSceneTexture& texture);
    ResourceDirectory sourceDirectory;
    ResourceDirectory outputDirectory;
    char              metadataFile[FS_MAX_PATH] = {};
    char              assetFile[FS_MAX_PATH] = {};
    const char*       pSourceFile;
    const char*       pGeometryFile;
    cJSON*            pMetadata = nullptr;
    cJSON*            pPrevious = nullptr;
    cJSON*            pAsset = nullptr;
    cJSON*            pTextureMetadata = nullptr;
    hz::Array<CookedSceneTexture> textures;
};
