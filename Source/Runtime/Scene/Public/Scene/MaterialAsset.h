/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/SceneAsset.h"

namespace hz
{
class AssetSource;
class AssetLoadersData;

class MaterialAsset
{
public:
    MaterialAsset() = default;
    ~MaterialAsset();
    MaterialAsset(const MaterialAsset&) = delete;
    MaterialAsset& operator=(const MaterialAsset&) = delete;

    const SceneAssetMaterial& getDescription() const;

private:
    AssetSource* pSource = nullptr;
    uint32_t     index = 0;
    friend class AssetLoadersData;
};
} // namespace hz
