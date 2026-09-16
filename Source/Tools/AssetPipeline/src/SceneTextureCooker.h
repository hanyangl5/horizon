/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Core/IContainer.h"
#include "Scene/SceneID.h"

struct cgltf_image;

struct CookedSceneTexture
{
    char        source[FS_MAX_PATH] = {};
    char        path[FS_MAX_PATH] = {};
    uint64_t    signature = 0;
    hz::AssetID registeredID;
    bool        srgb = false;
    bool        cooked = false;
};

class SceneTextureCooker
{
public:
    SceneTextureCooker(ResourceDirectory sourceDirectory, ResourceDirectory outputDirectory, const char* pSourceFile);
    bool cook(const cgltf_image& image, bool srgb, CookedSceneTexture& output) const;

private:
    bool readImage(const char* pPath, hz::Array<uint8_t>& bytes) const;
    bool writeDDS(const char* pPath, const void* pBytes, size_t size) const;

    ResourceDirectory sourceDirectory;
    ResourceDirectory outputDirectory;
    char              sourceParent[FS_MAX_PATH] = {};
};
