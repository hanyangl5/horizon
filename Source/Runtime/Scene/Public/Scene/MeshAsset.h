/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/SceneAsset.h"
#include "RHI/IGraphics.h"

namespace hz
{
class GPUBuffer;
class AssetSource;
class AssetGeometry;
class AssetLoadersData;

struct MeshBounds
{
    float minimum[3] = {};
    float maximum[3] = {};
};

class MeshAsset
{
public:
    MeshAsset() = default;
    ~MeshAsset();
    MeshAsset(const MeshAsset&) = delete;
    MeshAsset& operator=(const MeshAsset&) = delete;

    const SceneAssetMesh&             getDescription() const;
    const GPUBuffer&                  getIndexBuffer() const;
    // Vertex streams are dense, in ascending source-layout binding order.
    const GPUBuffer&                  getVertexBuffer(uint32_t binding) const;
    uint32_t                          getVertexBufferCount() const;
    uint32_t                          getVertexStride(uint32_t binding) const;
    IndexType                         getIndexType() const;
    const IndirectDrawIndexArguments& getDraw(uint32_t submesh) const;
    const MeshBounds&                 getBounds(uint32_t submesh) const;

private:
    AssetSource*   pSource = nullptr;
    AssetGeometry* pGeometry = nullptr;
    uint32_t       index = 0;
    friend class AssetLoadersData;
};
} // namespace hz
