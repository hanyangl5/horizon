#pragma once
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneAsset.h"
#include "Scene/SceneView.h"

namespace hz
{
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

// Selects the draws of one indirect list, like an SRP renderer list. Uploaded as-is for the renderer's culling shader.
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

constexpr uint32_t DrawListAlphaMode(MaterialAlphaMode Mode) { return 1u << (uint32_t)Mode; }

struct DrawGeometry
{
    GpuSubmeshRef Mesh;
    bool          Opaque = true;
    uint32_t      PreviousIndex = UINT32_MAX; // Geometry index in the preceding revision.
};

// Flattens the render scene into per-draw data and owns the indirect lists that the renderer's culling fills. Every list
// holds one indirect command per draw, with zero instances for draws its filter rejects, so draw ids stay valid across lists.
class GpuDrawList
{
public:
    static constexpr uint32_t MaxLists = 8;

    explicit GpuDrawList(RenderContext& Context);
    // Lists are registered at initialization.
    DrawListID         AddList(const DrawListFilter& Filter);
    // Builds and uploads this frame's draw data. The lists stay unwritten until a culling pass fills them.
    void               Prepare(const SceneView&);
    // Draw indices refer to the complete draw data.
    Span<GpuDrawData>  GetDraws() const { return { DrawData.GetData(), DrawData.Size() }; }
    Span<DrawGeometry> GetGeometries() const { return { Geometries.GetData(), Geometries.Size() }; }
    uint32_t           GetGeometryIndex(uint32_t DrawIndex) const { return GeometryIndices[DrawIndex]; }
    uint64_t           GetGeometryRevision() const { return GeometryRevision; }
    const GPUBuffer&   GetDrawData(uint32_t Backing) const { return Frames[Backing].DrawData; }
    const GPUBuffer&   GetCommands(uint32_t Backing, DrawListID List) const { return Frames[Backing].Lists[List.index]; }
    // The renderer's standard lists, registered at construction.
    DrawListID         GetOpaqueList() const { return OpaqueList; }
    DrawListID         GetTransparentList() const { return TransparentList; }
    DrawListID         GetShadowCasterList() const { return ShadowCasterList; }
    Dependencies       GetDependencies(const GPUBuffer& Frame, const SceneView& View, const GPUBuffer* Indirect = nullptr) const;
    // The registered filters with each list's UAV index filled in, one DrawListFilter per list.
    const GPUBuffer&   GetFilters(uint32_t Backing) const { return Frames[Backing].Filters; }
    uint32_t           GetListCount() const { return ListCount; }
    uint32_t           GetCount(uint32_t Backing) const { return Frames[Backing].Count; }

private:
    struct Frame
    {
        GPUBuffer                       DrawData;
        FixedArray<GPUBuffer, MaxLists> Lists;
        GPUBuffer                       Filters;
        uint32_t                        Capacity = 0;
        uint32_t                        Count = 0;
    };
    FixedArray<DrawListFilter, MaxLists> Filters;
    uint32_t                             ListCount = 0;
    DrawListID                           OpaqueList;
    DrawListID                           TransparentList;
    DrawListID                           ShadowCasterList;
    RenderContext&                       Context;
    FixedArray<Frame, 3>                 Frames;
    Array<GpuDrawData>                   DrawData;
    Array<DrawGeometry>                  Geometries;
    Array<uint32_t>                      GeometryIndices;
    uint64_t                             GeometryRevision = 0;
    Array<uint32_t>                      MaterialIds;
    uint64_t                             MaterialRevision = 0;
    mutable Array<const GPUBuffer*>      Buffers;
    mutable Array<const GPUTexture*>     Textures;
    mutable const GPUBuffer*             IndirectDependency = nullptr;
};
} // namespace hz
