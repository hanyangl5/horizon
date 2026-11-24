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

// The vertices a submesh's indices reference, in the shared vertex streams.
struct MeshVertexRange
{
    uint32_t first = 0;
    uint32_t Count = 0;
};

class MeshAsset
{
public:
    MeshAsset() = default;
    ~MeshAsset();
    MeshAsset(const MeshAsset&) = delete;
    MeshAsset& operator=(const MeshAsset&) = delete;

    const SceneAssetMesh&             GetDescription() const;
    const GPUBuffer&                  GetIndexBuffer() const;
    // One stream per source-layout binding. A stream the geometry lacks, such as texture coordinates or skin weights, is an
    // invalid buffer with a zero stride.
    const GPUBuffer&                  GetVertexBuffer(uint32_t binding) const;
    uint32_t                          GetVertexBufferCount() const;
    uint32_t                          GetVertexStride(uint32_t binding) const;
    IndexType                         GetIndexType() const;
    const IndirectDrawIndexArguments& GetDraw(uint32_t submesh) const;
    const MeshBounds&                 GetBounds(uint32_t submesh) const;
    const MeshVertexRange&            GetVertexRange(uint32_t submesh) const;

private:
    AssetSource*   pSource = nullptr;
    AssetGeometry* pGeometry = nullptr;
    uint32_t       index = 0;
    friend class AssetLoadersData;
};
} // namespace hz
