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
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const GpuMeshID&) const = default;
};
struct GpuMaterialID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
    bool     operator==(const GpuMaterialID&) const = default;
};
struct GpuTextureID
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
    bool     IsValid() const { return index != UINT32_MAX && generation != 0; }
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
    GpuMaterialID Id;
    uint32_t      BaseColorTexture = UINT32_MAX;
    uint32_t      NormalTexture = UINT32_MAX;
    uint32_t      MetallicRoughnessTexture = UINT32_MAX;
    uint32_t      EmissiveTexture = UINT32_MAX;
    float         BaseColorFactor[4] = { 1, 1, 1, 1 };
    float         EmissiveFactor[3] = {};
    float         MetallicFactor = 1;
    float         RoughnessFactor = 1;
    float         AlphaCutoff = 0.5f;
    uint32_t      AlphaMode = 0;
    uint32_t      DoubleSided = 0;
    uint32_t      Tags = 0;
};
// Shaders index this table by sizeof(GpuMaterialData); keep it in step with their GpuMaterialData struct.
static_assert(sizeof(GpuMaterialData) == 76);
struct GpuTextureData
{
    GpuTextureID id;
    uint32_t     srv = UINT32_MAX;
};

// CPU encoding of the ready payload Set; unused asset manager entries have invalid IDs.
// The srv fields are bindless descriptor indices resolved at encode time: they stay valid until
// GpuAssetTables::revision changes, so consumers that keep them must re-read this table afterwards.
struct GpuAssetTables
{
    Span<GpuMeshData>     meshes;
    Span<GpuVertexStream> vertexStreams;
    Span<GpuSubmeshData>  submeshes;
    Span<GpuMaterialData> materials;
    Span<GpuTextureData>  textures;
    uint64_t              revision = 0;

    const GpuMeshData* GetMesh(GpuMeshID id) const
    {
        return id.IsValid() && id.index < meshes.Count && meshes.Data[id.index].id == id ? meshes.Data + id.index : nullptr;
    }
    const GpuMaterialData* GetMaterial(GpuMaterialID id) const
    {
        return id.IsValid() && id.index < materials.Count && materials.Data[id.index].Id == id ? materials.Data + id.index : nullptr;
    }
    const GpuTextureData* GetTexture(GpuTextureID id) const
    {
        return id.IsValid() && id.index < textures.Count && textures.Data[id.index].id == id ? textures.Data + id.index : nullptr;
    }
};
} // namespace hz
