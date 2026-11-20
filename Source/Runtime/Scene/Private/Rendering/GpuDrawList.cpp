#include "Scene/GpuDrawList.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

namespace hz
{

GpuDrawList::GpuDrawList(RenderContext& Context): Context(Context)
{
    const uint32_t Opaque = DrawListAlphaMode(MaterialAlphaMode::Opaque) | DrawListAlphaMode(MaterialAlphaMode::Mask);
    OpaqueList = AddList({ .alphaModes = Opaque });
    TransparentList = AddList({ .alphaModes = DrawListAlphaMode(MaterialAlphaMode::Blend) });
    ShadowCasterList = AddList({ .alphaModes = Opaque, .frustumCull = 0 });
}

static_assert(sizeof(DrawListFilter) == 4 * sizeof(uint32_t));

DrawListID GpuDrawList::AddList(const DrawListFilter& Filter)
{
    ASSERT(ListCount < MaxLists && Filter.alphaModes);
    Filters[ListCount] = Filter;
    // Frames were sized for the previous lists; the next Prepare recreates them.
    for (Frame& Backing : Frames)
    {
        Backing.Capacity = 0;
    }
    return { ListCount++ };
}

void GpuDrawList::Prepare(const SceneView& View)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuDrawList::Prepare");
    const GpuAssetTables& Tables = View.GpuScene.GetAssetTables();
    if (MaterialRevision != Tables.revision)
    {
        MaterialIds.Resize(Tables.materials.Count);
        uint32_t Id = 0;
        for (uint32_t Index = 0; Index < Tables.materials.Count; ++Index)
        {
            MaterialIds[Index] = Tables.materials.Data[Index].id.IsValid() ? Id++ : UINT32_MAX;
        }
        MaterialRevision = Tables.revision;
    }
    DrawData.Clear();
    GeometryIndices.Clear();
    uint32_t   GeometryCount = 0;
    bool       GeometryChanged = false;
    const auto MarkGeometryChanged = [&]
    {
        if (!GeometryChanged)
        {
            for (uint32_t Index = 0; Index < Geometries.Size(); ++Index)
            {
                Geometries[Index].PreviousIndex = Index;
            }
            GeometryChanged = true;
        }
    };
    for (uint32_t MeshBatchIndex : View.Scene.GetActiveMeshBatches())
    {
        const RenderMeshBatch& Draw = View.Scene.GetMeshBatches().Data[MeshBatchIndex];
        const RenderPrimitive& Primitive = View.Scene.GetPrimitives().Data[Draw.primitive.index];
        const GpuMeshData*     Mesh = Tables.GetMesh(Draw.mesh.mesh);
        const GpuMaterialData* Material = Tables.GetMaterial(Draw.material);
        if (!Primitive.instanceCount || !Mesh || !Material)
        {
            continue;
        }
        // The first three bindings are position, normal and texture coordinates; only positions are required.
        ASSERT(Mesh->vertexStreamCount >= 3 && Tables.vertexStreams.Data[Mesh->vertexStreamOffset].srv != UINT32_MAX &&
               Primitive.id == Draw.primitive);
        const GpuSubmeshData& Submesh = Tables.submeshes.Data[Mesh->submeshOffset + Draw.mesh.submeshIndex];
        uint32_t              GeometryIndex = UINT32_MAX;
        if (Material->alphaMode != uint32_t(MaterialAlphaMode::Blend))
        {
            const bool Opaque = Material->alphaMode == uint32_t(MaterialAlphaMode::Opaque);
            GeometryIndex = 0;
            while (GeometryIndex < Geometries.Size() &&
                   (Geometries[GeometryIndex].Mesh != Draw.mesh || Geometries[GeometryIndex].Opaque != Opaque))
            {
                ++GeometryIndex;
            }
            if (GeometryIndex >= GeometryCount)
            {
                if (GeometryIndex != GeometryCount || GeometryIndex == Geometries.Size())
                {
                    MarkGeometryChanged();
                }
                if (GeometryIndex == Geometries.Size())
                {
                    Geometries.PushBack({ .Mesh = Draw.mesh, .Opaque = Opaque });
                }
                std::swap(Geometries[GeometryIndex], Geometries[GeometryCount]);
                GeometryIndex = GeometryCount++;
            }
        }
        for (uint32_t Index = 0; Index < Primitive.instanceCount; ++Index)
        {
            GeometryIndices.PushBack(GeometryIndex);
            DrawData.PushBack({ .instance = View.Scene.GetInstanceIndices().Data[Primitive.instanceOffset + Index],
                                .material = Draw.material.index,
                                .materialId = MaterialIds[Draw.material.index],
                                .alphaMode = Material->alphaMode,
                                .positions = Tables.vertexStreams.Data[Mesh->vertexStreamOffset].srv,
                                .normals = Tables.vertexStreams.Data[Mesh->vertexStreamOffset + 1].srv,
                                .texcoords = Tables.vertexStreams.Data[Mesh->vertexStreamOffset + 2].srv,
                                .indices = Mesh->indexSrv,
                                .indexStride = Mesh->indexStride,
                                .firstIndex = Submesh.firstIndex,
                                .vertexOffset = Submesh.vertexOffset,
                                .indexCount = Submesh.indexCount });
        }
    }
    if (GeometryCount != Geometries.Size())
    {
        MarkGeometryChanged();
        Geometries.Resize(GeometryCount);
    }
    if (GeometryChanged)
    {
        ++GeometryRevision;
    }
    Frame& CurrentFrame = Frames[View.FrameIndex];
    CurrentFrame.Count = DrawData.Size();
    if (CurrentFrame.Count > CurrentFrame.Capacity || !CurrentFrame.Filters.IsValid())
    {
        CurrentFrame.Capacity = TF_MAX(64u, TF_MAX(CurrentFrame.Count, CurrentFrame.Capacity * 2));
        CurrentFrame.DrawData = Context.CreateBuffer({ .Size = uint64_t(CurrentFrame.Capacity) * sizeof(GpuDrawData),
                                                       .ElementCount = CurrentFrame.Capacity * (uint32_t)(sizeof(GpuDrawData) / 4),
                                                       .Name = "Renderer.DrawData",
                                                       .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                       .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                       .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
                                                       .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        const BufferDesc Arguments = { .Size = uint64_t(CurrentFrame.Capacity) * 20,
                                       .ElementCount = CurrentFrame.Capacity * 5,
                                       .Name = "Renderer.IndirectDraws",
                                       .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                       .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                       .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW | DESCRIPTOR_TYPE_INDIRECT_BUFFER };
        for (uint32_t List = 0; List < ListCount; ++List)
        {
            CurrentFrame.Lists[List] = Context.CreateBuffer(Arguments);
        }
        CurrentFrame.Filters = Context.CreateBuffer({ .Size = sizeof(DrawListFilter) * MaxLists,
                                                      .ElementCount = MaxLists,
                                                      .StructStride = sizeof(DrawListFilter),
                                                      .Name = "Renderer.DrawListFilters",
                                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                      .Descriptors = DESCRIPTOR_TYPE_BUFFER,
                                                      .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(CurrentFrame.DrawData.IsValid() && CurrentFrame.Filters.IsValid());
        DrawListFilter Uploaded[MaxLists] = {};
        for (uint32_t List = 0; List < ListCount; ++List)
        {
            Uploaded[List] = Filters[List];
            Uploaded[List].commandsIndex = CurrentFrame.Lists[List].GetUavIndex();
        }
        const bool Written = Context.UpdateBuffer(CurrentFrame.Filters, 0, Uploaded, sizeof(Uploaded));
        ASSERT(Written);
    }
    if (!CurrentFrame.Count)
    {
        return;
    }
    const bool Uploaded =
        Context.UpdateBuffer(CurrentFrame.DrawData, 0, DrawData.GetData(), uint64_t(CurrentFrame.Count) * sizeof(GpuDrawData));
    ASSERT(Uploaded);
}

Dependencies GpuDrawList::GetDependencies(const GPUBuffer& Frame, const SceneView& View, const GPUBuffer* Indirect) const
{
    Buffers.Clear();
    Textures.Clear();
    Buffers.PushBack(&Frame);
    Buffers.PushBack(&GetDrawData(View.FrameIndex));
    const GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
    if (Gpu.instances.IsValid())
    {
        Buffers.PushBack(&Gpu.instances);
    }
    if (Gpu.materials.IsValid())
    {
        Buffers.PushBack(&Gpu.materials);
    }
    if (Gpu.textures.IsValid())
    {
        Buffers.PushBack(&Gpu.textures);
    }
    const GpuAssetTables& Tables = View.GpuScene.GetAssetTables();
    const GPUBuffer*      Previous = nullptr;
    for (const GpuMeshData& MeshData : Tables.meshes)
    {
        if (!MeshData.id.IsValid())
        {
            continue;
        }
        const MeshAsset* Mesh = View.Assets.GetMesh({ .index = MeshData.id.index, .generation = MeshData.id.generation });
        ASSERT(Mesh);
        if (&Mesh->GetIndexBuffer() == Previous)
        {
            continue;
        }
        Previous = &Mesh->GetIndexBuffer();
        Buffers.PushBack(Previous);
        for (uint32_t Index = 0; Index < Mesh->GetVertexBufferCount(); ++Index)
        {
            if (Mesh->GetVertexBuffer(Index).IsValid())
            {
                Buffers.PushBack(&Mesh->GetVertexBuffer(Index));
            }
        }
    }
    for (const GpuTextureData& TextureData : Tables.textures)
    {
        if (TextureData.id.IsValid())
        {
            Textures.PushBack(
                &View.Assets.GetTexture({ .index = TextureData.id.index, .generation = TextureData.id.generation })->GetTexture());
        }
    }
    IndirectDependency = Indirect;
    return { .sampledTextures = { Textures.GetData(), Textures.Size() },
             .buffers = { Buffers.GetData(), Buffers.Size() },
             .indirectBuffers = { &IndirectDependency, Indirect ? 1u : 0u } };
}
} // namespace hz
