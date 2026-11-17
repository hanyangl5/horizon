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

// Selects the draws of one indirect list, like an SRP renderer list. Uploaded to the culling shader as-is.
struct DrawListFilter
{
    // Bit (1 << MaterialAlphaMode) for every alpha mode the list accepts.
    uint32_t alphaModes = 0;
    // The material must carry every one of these tag bits.
    uint32_t requiredTags = 0;
    // Shadow casters outside the camera frustum still cast, so their list keeps uncullable draws.
    uint32_t frustumCull = 1;
    uint32_t commandsIndex = UINT32_MAX;
};

struct DrawListID
{
    uint32_t index = UINT32_MAX;
    bool     IsValid() const { return index != UINT32_MAX; }
};

constexpr uint32_t DrawListAlphaMode(hz::MaterialAlphaMode Mode) { return 1u << (uint32_t)Mode; }

struct DrawGeometry
{
    hz::GpuSubmeshRef Mesh;
    bool              Opaque = true;
    uint32_t          PreviousIndex = UINT32_MAX; // Geometry index in the preceding revision.
};

class GpuDrawList
{
public:
    static constexpr uint32_t MaxLists = 8;

    explicit GpuDrawList(hz::RenderContext& Context);
    // Lists are registered at initialization; every list holds one indirect command per draw, with zero instances for draws
    // its filter rejects, so draw ids stay valid across lists.
    DrawListID            AddList(const DrawListFilter& Filter);
    void                  Prepare(hz::CommandList&, const hz::GPUBuffer& Frame, const SceneView&, bool Culling = true);
    // Draw indices refer to the complete list before GPU frustum culling.
    hz::Span<GpuDrawData>  GetDraws() const { return { DrawData.GetData(), DrawData.Size() }; }
    hz::Span<DrawGeometry> GetGeometries() const { return { Geometries.GetData(), Geometries.Size() }; }
    uint32_t               GetGeometryIndex(uint32_t DrawIndex) const { return GeometryIndices[DrawIndex]; }
    uint64_t               GetGeometryRevision() const { return GeometryRevision; }
    const hz::GPUBuffer&   GetDrawData(uint32_t Backing) const { return Frames[Backing].DrawData; }
    const hz::GPUBuffer&   GetCommands(uint32_t Backing, DrawListID List) const { return Frames[Backing].Lists[List.index]; }
    // The renderer's standard lists, registered at construction.
    DrawListID             GetOpaqueList() const { return OpaqueList; }
    DrawListID             GetTransparentList() const { return TransparentList; }
    DrawListID             GetShadowCasterList() const { return ShadowCasterList; }
    hz::Dependencies     GetDependencies(const hz::GPUBuffer& Frame, const SceneView& View,
                                         const hz::GPUBuffer* Indirect = nullptr) const;
    const hz::GPUBuffer& GetStatistics(uint32_t Backing) const { return Frames[Backing].Statistics; }
    uint32_t             GetCount(uint32_t Backing) const { return Frames[Backing].Count; }

private:
    struct Frame
    {
        hz::GPUBuffer                           DrawData;
        hz::FixedArray<hz::GPUBuffer, MaxLists> Lists;
        hz::GPUBuffer                           Filters;
        hz::GPUBuffer                           Statistics;
        uint32_t                                Capacity = 0;
        uint32_t                                Count = 0;
    };
    hz::FixedArray<DrawListFilter, MaxLists> Filters;
    uint32_t                                 ListCount = 0;
    DrawListID                               OpaqueList;
    DrawListID                               TransparentList;
    DrawListID                               ShadowCasterList;
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
