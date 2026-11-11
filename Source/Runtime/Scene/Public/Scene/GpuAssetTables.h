/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ISpan.h"

namespace hz
{
// Typed asset manager index identities, independent of RHI descriptor indices.
struct GpuMeshID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const GpuMeshID&) const = default;
};
struct GpuMaterialID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const GpuMaterialID&) const = default;
};
struct GpuTextureID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     isValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const GpuTextureID&) const = default;
};
struct GpuSubmeshRef
{
    GpuMeshID mesh;
    uint32_t  submeshIndex = 0;
    bool      operator==(const GpuSubmeshRef&) const = default;
};
struct GpuMeshData
{
    GpuMeshID id;
    uint32_t  indexSrv = UINT32_MAX;
    uint32_t  indexStride = 0;
    uint32_t  vertexStreamOffset = 0;
    uint32_t  vertexStreamCount = 0;
    uint32_t  submeshOffset = 0;
    uint32_t  submeshCount = 0;
};
struct GpuVertexStream
{
    uint32_t srv = UINT32_MAX;
    uint32_t stride = 0;
};
struct GpuSubmeshData
{
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t  vertexOffset = 0;
};
struct GpuMaterialData
{
    GpuMaterialID id;
    uint32_t      baseColorTexture = UINT32_MAX;
    uint32_t      normalTexture = UINT32_MAX;
    uint32_t      metallicRoughnessTexture = UINT32_MAX;
    uint32_t      emissiveTexture = UINT32_MAX;
    float         baseColorFactor[4] = { 1, 1, 1, 1 };
    float         emissiveFactor[3] = {};
    float         metallicFactor = 1;
    float         roughnessFactor = 1;
    float         alphaCutoff = 0.5f;
    uint32_t      alphaMode = 0;
    uint32_t      doubleSided = 0;
};
struct GpuTextureData
{
    GpuTextureID id;
    uint32_t     srv = UINT32_MAX;
};

// CPU encoding of the ready payload set; unused asset manager entries have invalid IDs.
struct GpuAssetTables
{
    Span<GpuMeshData>       meshes;
    Span<GpuVertexStream>   vertexStreams;
    Span<GpuSubmeshData>    submeshes;
    Span<GpuMaterialData>   materials;
    Span<GpuTextureData>    textures;
    uint64_t                revision = 0;

    const GpuMeshData* getMesh(GpuMeshID id) const
    {
        return id.isValid() && id.index < meshes.count && meshes.pData[id.index].id == id ? meshes.pData + id.index : nullptr;
    }
    const GpuMaterialData* getMaterial(GpuMaterialID id) const
    {
        return id.isValid() && id.index < materials.count && materials.pData[id.index].id == id ? materials.pData + id.index : nullptr;
    }
    const GpuTextureData* getTexture(GpuTextureID id) const
    {
        return id.isValid() && id.index < textures.count && textures.pData[id.index].id == id ? textures.pData + id.index : nullptr;
    }
};
} // namespace hz
