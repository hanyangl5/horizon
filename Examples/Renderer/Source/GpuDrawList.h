#pragma once
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "SceneView.h"

// Uploaded to shaders as-is, so the fields keep the shader's names.
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
    hz::GpuSubmeshRef Mesh;
    bool              Opaque = true;
    uint32_t          PreviousIndex = UINT32_MAX; // Geometry index in the preceding revision.
};

class GpuDrawList
{
public:
    explicit GpuDrawList(hz::RenderContext& Context);
    void                  Prepare(hz::CommandList&, const hz::GPUBuffer& Frame, const SceneView&, bool Culling = true);
    // Draw indices refer to the complete list before GPU frustum culling.
    hz::Span<GpuDrawData>  GetDraws() const { return { DrawData.GetData(), DrawData.Size() }; }
    hz::Span<DrawGeometry> GetGeometries() const { return { Geometries.GetData(), Geometries.Size() }; }
    uint32_t               GetGeometryIndex(uint32_t DrawIndex) const { return GeometryIndices[DrawIndex]; }
    uint64_t               GetGeometryRevision() const { return GeometryRevision; }
    const hz::GPUBuffer&   GetDrawData(uint32_t Backing) const { return Frames[Backing].DrawData; }
    const hz::GPUBuffer&   GetCommands(uint32_t Backing, bool Transparent = false) const
    {
        return Transparent ? Frames[Backing].Transparent : Frames[Backing].Opaque;
    }
    const hz::GPUBuffer& GetShadowCommands(uint32_t Backing) const { return Frames[Backing].Shadow; }
    hz::Dependencies     GetDependencies(const hz::GPUBuffer& Frame, const SceneView& View,
                                         const hz::GPUBuffer* Indirect = nullptr) const;
    const hz::GPUBuffer& GetStatistics(uint32_t Backing) const { return Frames[Backing].Statistics; }
    uint32_t             GetCount(uint32_t Backing) const { return Frames[Backing].Count; }

private:
    struct Frame
    {
        hz::GPUBuffer DrawData;
        hz::GPUBuffer Opaque;
        hz::GPUBuffer Transparent;
        hz::GPUBuffer Shadow;
        hz::GPUBuffer Statistics;
        uint32_t      Capacity = 0;
        uint32_t      Count = 0;
    };
    hz::RenderContext&                       Context;
    hz::GPUPipeline                          Pipeline;
    hz::GPUPipeline                          ClearPipeline;
    hz::FixedArray<Frame, 3>                 Frames;
    hz::Array<GpuDrawData>                   DrawData;
    hz::Array<DrawGeometry>                  Geometries;
    hz::Array<uint32_t>                      GeometryIndices;
    uint64_t                                 GeometryRevision = 0;
    hz::Array<uint32_t>                      MaterialIds;
    uint64_t                                 MaterialRevision = 0;
    mutable hz::Array<const hz::GPUBuffer*>  Buffers;
    mutable hz::Array<const hz::GPUTexture*> Textures;
    mutable const hz::GPUBuffer*             IndirectDependency = nullptr;
};
