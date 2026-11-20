/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Graphics/RenderContext.h"

namespace hz
{
struct GpuSkinningDesc
{
    // Source-layout bindings of the streams the skinning shader reads. Skinned streams keep the source strides.
    uint32_t positionBinding = 0;
    uint32_t normalBinding = 1;
    uint32_t texcoordBinding = 2;
    uint32_t jointBinding = 3;
    uint32_t weightBinding = 4;
    // Frames in flight, each with its own joint palette and job table.
    uint32_t frameCount = 3;
};

// One skinned instance for the skinning shader, uploaded as-is, so the fields keep the shader's names. Vertices
// firstVertex + v, for v below vertexCount, are read from the source streams and written to the same index of the
// skinned streams.
struct GpuSkinningJob
{
    uint32_t positions;
    uint32_t normals;
    uint32_t texcoords; // UINT32_MAX when the mesh has none.
    uint32_t joints;
    uint32_t weights;
    uint32_t outputPositions;
    uint32_t outputNormals;
    uint32_t outputTexcoords;
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t jointOffset;
    uint32_t padding;
};

// The streams a skinned instance renders and traces from. They span the shared mesh streams up to the last vertex the
// mesh references, because cooked indices are absolute; only the referenced range is written.
struct GpuSkinnedVertices
{
    RenderInstanceID instance;
    GpuMeshID        mesh;
    uint32_t         vertexCount = 0;
    GPUBuffer        positions;
    GPUBuffer        normals;
    GPUBuffer        texcoords;
};

class GpuSkinningData;

// GPU side of skinning: a ring of per-frame joint palettes holding RenderScene's skinning matrices, and per skinned
// instance the streams an application skinning pass writes. Instances whose mesh lacks joint or weight streams keep
// rendering their mesh streams.
class GpuSkinning
{
public:
    // Scene, asset manager and context must outlive this object.
    GpuSkinning(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSkinningDesc& desc = {});
    ~GpuSkinning();
    GpuSkinning(const GpuSkinning&) = delete;
    GpuSkinning& operator=(const GpuSkinning&) = delete;

    // Writes the frame's palette and jobs after RenderScene::Update. The caller ensures the GPU no longer reads the frame
    // slot. Allocates only when skinned instances appear or the palette grows.
    void                      Prepare(uint32_t frame);
    const GPUBuffer&          GetPalette(uint32_t frame) const;
    const GPUBuffer&          GetJobs(uint32_t frame) const;
    uint32_t                  GetJobCount() const;
    uint32_t                  GetMaxVertexCount() const;
    // Null when the instance renders its mesh streams.
    const GpuSkinnedVertices* Find(RenderInstanceID instance) const;
    Span<GpuSkinnedVertices>  GetSkinnedVertices() const;
    // The dispatch reads the palette, the jobs and the source streams and writes the skinned streams.
    Dependencies              GetDependencies(uint32_t frame) const;

private:
    GpuSkinningData* Data = nullptr;
};
} // namespace hz
