#include "GpuDrawList.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

GpuDrawList::GpuDrawList(hz::RenderContext& Context): Context(Context)
{
    Pipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain", .Name = "Renderer.FrustumCull" } },
                          .pFileName = "FrustumCulling.slang" },
          .Name = "Renderer.FrustumCull" });
    ClearPipeline =
        Context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "ClearStatistics" } },
                                                        .pFileName = "FrustumCulling.slang" },
                                        .Name = "Renderer.ClearCullingStatistics" });
    ASSERT(Pipeline.IsValid() && ClearPipeline.IsValid());
    const uint32_t Opaque = DrawListAlphaMode(hz::MaterialAlphaMode::Opaque) | DrawListAlphaMode(hz::MaterialAlphaMode::Mask);
    OpaqueList = AddList({ .alphaModes = Opaque });
    TransparentList = AddList({ .alphaModes = DrawListAlphaMode(hz::MaterialAlphaMode::Blend) });
    ShadowCasterList = AddList({ .alphaModes = Opaque, .frustumCull = 0 });
}

// Mirrors the RootConstant0 block of FrustumCulling.slang field for field.
struct CullingConstants
{
    uint32_t frameIndex;
    uint32_t drawsIndex;
    uint32_t instancesIndex;
    uint32_t materialsIndex;
    uint32_t filtersIndex;
    uint32_t filterCount;
    uint32_t statisticsIndex;
    uint32_t drawCount;
    uint32_t cullingEnabled;
};
static_assert(sizeof(CullingConstants) == 9 * sizeof(uint32_t));
static_assert(sizeof(DrawListFilter) == 4 * sizeof(uint32_t));
// Offset of statisticsIndex, which ClearStatistics reads on its own.
constexpr uint32_t CullingStatisticsSlot = offsetof(CullingConstants, statisticsIndex) / sizeof(uint32_t);

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

void GpuDrawList::Prepare(hz::CommandList& Commands, const hz::GPUBuffer& FrameBuffer, const SceneView& View, bool Culling)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuDrawList::Prepare");
    const hz::GpuAssetTables& Tables = View.GpuScene.GetAssetTables();
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
        const hz::RenderMeshBatch& Draw = View.Scene.GetMeshBatches().Data[MeshBatchIndex];
        const hz::RenderPrimitive& Primitive = View.Scene.GetPrimitives().Data[Draw.primitive.index];
        const hz::GpuMeshData*     Mesh = Tables.GetMesh(Draw.mesh.mesh);
        const hz::GpuMaterialData* Material = Tables.GetMaterial(Draw.material);
        if (!Primitive.instanceCount || !Mesh || !Material)
        {
            continue;
        }
        ASSERT(Mesh->vertexStreamCount == 3 && Primitive.id == Draw.primitive);
        const hz::GpuSubmeshData& Submesh = Tables.submeshes.Data[Mesh->submeshOffset + Draw.mesh.submeshIndex];
        uint32_t                  GeometryIndex = UINT32_MAX;
        if (Material->alphaMode != uint32_t(hz::MaterialAlphaMode::Blend))
        {
            const bool Opaque = Material->alphaMode == uint32_t(hz::MaterialAlphaMode::Opaque);
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
    if (CurrentFrame.Count > CurrentFrame.Capacity || !CurrentFrame.Statistics.IsValid())
    {
        CurrentFrame.Capacity = TF_MAX(64u, TF_MAX(CurrentFrame.Count, CurrentFrame.Capacity * 2));
        CurrentFrame.DrawData = Context.CreateBuffer({ .Size = uint64_t(CurrentFrame.Capacity) * sizeof(GpuDrawData),
                                                       .ElementCount = CurrentFrame.Capacity * (uint32_t)(sizeof(GpuDrawData) / 4),
                                                       .Name = "Renderer.DrawData",
                                                       .usage = MemoryUsageCpuToGpu,
                                                       .StartState = ResourceStateShaderResource,
                                                       .Descriptors = DescriptorTypeBufferRaw,
                                                       .Flags = BufferCreationFlagPersistentMapBit });
        const hz::BufferDesc Arguments = { .Size = uint64_t(CurrentFrame.Capacity) * 20,
                                           .ElementCount = CurrentFrame.Capacity * 5,
                                           .Name = "Renderer.IndirectDraws",
                                           .usage = MemoryUsageGpuOnly,
                                           .StartState = ResourceStateUnorderedAccess,
                                           .Descriptors = DescriptorTypeRwBufferRaw | DescriptorTypeIndirectBuffer };
        for (uint32_t List = 0; List < ListCount; ++List)
        {
            CurrentFrame.Lists[List] = Context.CreateBuffer(Arguments);
        }
        CurrentFrame.Filters = Context.CreateBuffer({ .Size = sizeof(DrawListFilter) * MaxLists,
                                                      .ElementCount = MaxLists,
                                                      .StructStride = sizeof(DrawListFilter),
                                                      .Name = "Renderer.DrawListFilters",
                                                      .usage = MemoryUsageCpuToGpu,
                                                      .StartState = ResourceStateShaderResource,
                                                      .Descriptors = DescriptorTypeBuffer,
                                                      .Flags = BufferCreationFlagPersistentMapBit });
        CurrentFrame.Statistics = Context.CreateBuffer({ .Size = 16,
                                                         .ElementCount = 4,
                                                         .Name = "Renderer.CullingStatistics",
                                                         .usage = MemoryUsageGpuOnly,
                                                         .StartState = ResourceStateUnorderedAccess,
                                                         .Descriptors = DescriptorTypeRwBufferRaw });
        ASSERT(CurrentFrame.DrawData.IsValid() && CurrentFrame.Filters.IsValid() && CurrentFrame.Statistics.IsValid());
        DrawListFilter Uploaded[MaxLists] = {};
        for (uint32_t List = 0; List < ListCount; ++List)
        {
            Uploaded[List] = Filters[List];
            Uploaded[List].commandsIndex = CurrentFrame.Lists[List].GetUavIndex();
        }
        const bool Written = Context.UpdateBuffer(CurrentFrame.Filters, 0, Uploaded, sizeof(Uploaded));
        ASSERT(Written);
    }
    Commands.SetPipeline(ClearPipeline);
    Commands.SetRootConstant({ CurrentFrame.Statistics.GetUavIndex() }, CullingStatisticsSlot);
    Commands.Dispatch(1, 1, 1, { .storageBuffers = { &CurrentFrame.Statistics } });
    if (!CurrentFrame.Count)
    {
        return;
    }
    const bool Uploaded =
        Context.UpdateBuffer(CurrentFrame.DrawData, 0, DrawData.GetData(), uint64_t(CurrentFrame.Count) * sizeof(GpuDrawData));
    ASSERT(Uploaded);
    Commands.BeginGpuTimestamp("GPU Frustum Culling");
    Commands.SetPipeline(Pipeline);
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
    const CullingConstants     Binding = { .frameIndex = FrameBuffer.GetSrvIndex(),
                                           .drawsIndex = CurrentFrame.DrawData.GetSrvIndex(),
                                           .instancesIndex = Gpu.instances.GetSrvIndex(),
                                           .materialsIndex = Gpu.materials.GetSrvIndex(),
                                           .filtersIndex = CurrentFrame.Filters.GetSrvIndex(),
                                           .filterCount = ListCount,
                                           .statisticsIndex = CurrentFrame.Statistics.GetUavIndex(),
                                           .drawCount = CurrentFrame.Count,
                                           .cullingEnabled = Culling ? 1u : 0u };
    Commands.SetRootConstants(Binding);
    const hz::GPUBuffer* Outputs[MaxLists + 1] = { &CurrentFrame.Statistics };
    for (uint32_t List = 0; List < ListCount; ++List)
    {
        Outputs[List + 1] = &CurrentFrame.Lists[List];
    }
    Commands.Dispatch((CurrentFrame.Count + 63) / 64, 1, 1,
                      { .buffers = { &FrameBuffer, &CurrentFrame.DrawData, &Gpu.instances, &Gpu.materials, &CurrentFrame.Filters },
                        .storageBuffers = { Outputs, ListCount + 1 } });
    Commands.EndGpuTimestamp();
}

hz::Dependencies GpuDrawList::GetDependencies(const hz::GPUBuffer& Frame, const SceneView& View, const hz::GPUBuffer* Indirect) const
{
    Buffers.Clear();
    Textures.Clear();
    Buffers.PushBack(&Frame);
    Buffers.PushBack(&GetDrawData(View.FrameIndex));
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
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
    const hz::GpuAssetTables& Tables = View.GpuScene.GetAssetTables();
    const hz::GPUBuffer*      Previous = nullptr;
    for (const hz::GpuMeshData& MeshData : Tables.meshes)
    {
        if (!MeshData.id.IsValid())
        {
            continue;
        }
        const hz::MeshAsset* Mesh = View.Assets.GetMesh({ .index = MeshData.id.index, .generation = MeshData.id.generation });
        ASSERT(Mesh);
        if (&Mesh->GetIndexBuffer() == Previous)
        {
            continue;
        }
        Previous = &Mesh->GetIndexBuffer();
        Buffers.PushBack(Previous);
        for (uint32_t Index = 0; Index < Mesh->GetVertexBufferCount(); ++Index)
        {
            Buffers.PushBack(&Mesh->GetVertexBuffer(Index));
        }
    }
    for (const hz::GpuTextureData& TextureData : Tables.textures)
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
