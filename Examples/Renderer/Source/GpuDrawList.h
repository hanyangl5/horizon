#pragma once
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "SceneView.h"

struct GpuDrawData
{
    uint32_t instance;
    uint32_t material;
    uint32_t materialId;
    uint32_t alphaMode;
    uint32_t positions;
    uint32_t normals;
    uint32_t texcoords;
    uint32_t indices;
    uint32_t indexStride;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t indexCount;
};

struct DrawGeometry
{
    hz::GpuSubmeshRef mesh;
    bool              opaque = true;
    uint32_t          previousIndex = UINT32_MAX; // Geometry index in the preceding revision.
};

class GpuDrawList
{
public:
    explicit GpuDrawList(hz::RenderContext& context);
    void                 prepare(hz::CommandList&, const hz::GPUBuffer& frame, const SceneView&, bool culling = true);
    // Draw indices refer to the complete list before GPU frustum culling.
    hz::Span<GpuDrawData>  getDraws() const { return { drawData.data(), drawData.size() }; }
    hz::Span<DrawGeometry> getGeometries() const { return { geometries.data(), geometries.size() }; }
    uint32_t               getGeometryIndex(uint32_t drawIndex) const { return geometryIndices[drawIndex]; }
    uint64_t               getGeometryRevision() const { return geometryRevision; }
    const hz::GPUBuffer& getDrawData(uint32_t backing) const { return frames[backing].drawData; }
    const hz::GPUBuffer& getCommands(uint32_t backing, bool transparent = false) const
    {
        return transparent ? frames[backing].transparent : frames[backing].opaque;
    }
    const hz::GPUBuffer& getShadowCommands(uint32_t backing) const { return frames[backing].shadow; }
    hz::Dependencies     getDependencies(const hz::GPUBuffer& frame, const SceneView& view, const hz::GPUBuffer* indirect = nullptr) const;
    const hz::GPUBuffer& getStatistics(uint32_t backing) const { return frames[backing].statistics; }
    uint32_t             getCount(uint32_t backing) const { return frames[backing].count; }

private:
    struct Frame
    {
        hz::GPUBuffer drawData;
        hz::GPUBuffer opaque;
        hz::GPUBuffer transparent;
        hz::GPUBuffer shadow;
        hz::GPUBuffer statistics;
        uint32_t      capacity = 0;
        uint32_t      count = 0;
    };
    hz::RenderContext&                       context;
    hz::GPUPipeline                          pipeline;
    hz::GPUPipeline                          clearPipeline;
    hz::FixedArray<Frame, 3>                 frames;
    hz::Array<GpuDrawData>                   drawData;
    hz::Array<DrawGeometry>                  geometries;
    hz::Array<uint32_t>                      geometryIndices;
    uint64_t                                 geometryRevision = 0;
    hz::Array<uint32_t>                      materialIds;
    uint64_t                                 materialRevision = 0;
    mutable hz::Array<const hz::GPUBuffer*>  buffers;
    mutable hz::Array<const hz::GPUTexture*> textures;
    mutable const hz::GPUBuffer*             indirectDependency = nullptr;
};
