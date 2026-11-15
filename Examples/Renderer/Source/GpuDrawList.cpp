#include "GpuDrawList.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

GpuDrawList::GpuDrawList(hz::RenderContext& Context): Context(Context)
{
    Pipeline = Context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain", .pName = "Renderer.FrustumCull" } },
                          .pFileName = "FrustumCulling.slang" },
          .pName = "Renderer.FrustumCull" });
    ClearPipeline =
        Context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearStatistics" } },
                                                        .pFileName = "FrustumCulling.slang" },
                                        .pName = "Renderer.ClearCullingStatistics" });
    ASSERT(Pipeline.isValid() && ClearPipeline.isValid());
}

// Mirrors the RootConstant0 block of FrustumCulling.slang field for field.
struct CullingConstants
{
    uint32_t frameIndex;
    uint32_t drawsIndex;
    uint32_t instancesIndex;
    uint32_t opaqueIndex;
    uint32_t transparentIndex;
    uint32_t statisticsIndex;
    uint32_t drawCount;
    uint32_t cullingEnabled;
    uint32_t shadowsIndex;
};
static_assert(sizeof(CullingConstants) == 9 * sizeof(uint32_t));

void GpuDrawList::Prepare(hz::CommandList& Commands, const hz::GPUBuffer& FrameBuffer, const SceneView& View, bool Culling)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuDrawList::Prepare");
    const hz::GpuAssetTables& Tables = View.GpuScene.getAssetTables();
    if (MaterialRevision != Tables.revision)
    {
        MaterialIds.resize(Tables.materials.Count);
        uint32_t Id = 0;
        for (uint32_t Index = 0; Index < Tables.materials.Count; ++Index)
        {
            MaterialIds[Index] = Tables.materials.Data[Index].id.isValid() ? Id++ : UINT32_MAX;
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
    for (uint32_t MeshBatchIndex : View.Scene.getActiveMeshBatches())
    {
        const hz::RenderMeshBatch& Draw = View.Scene.getMeshBatches().Data[MeshBatchIndex];
        const hz::RenderPrimitive& Primitive = View.Scene.getPrimitives().Data[Draw.primitive.index];
        const hz::GpuMeshData*     Mesh = Tables.getMesh(Draw.mesh.mesh);
        const hz::GpuMaterialData* Material = Tables.getMaterial(Draw.material);
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
            DrawData.PushBack({ .instance = View.Scene.getInstanceIndices().Data[Primitive.instanceOffset + Index],
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
        Geometries.resize(GeometryCount);
    }
    if (GeometryChanged)
    {
        ++GeometryRevision;
    }
    Frame& CurrentFrame = Frames[View.FrameIndex];
    CurrentFrame.Count = DrawData.Size();
    if (CurrentFrame.Count > CurrentFrame.Capacity || !CurrentFrame.Statistics.isValid())
    {
        CurrentFrame.Capacity = TF_MAX(64u, TF_MAX(CurrentFrame.Count, CurrentFrame.Capacity * 2));
        CurrentFrame.DrawData = Context.createBuffer({ .Size = uint64_t(CurrentFrame.Capacity) * sizeof(GpuDrawData),
                                                       .elementCount = CurrentFrame.Capacity * (uint32_t)(sizeof(GpuDrawData) / 4),
                                                       .pName = "Renderer.DrawData",
                                                       .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                       .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                       .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        const hz::BufferDesc Arguments = { .Size = uint64_t(CurrentFrame.Capacity) * 20,
                                           .elementCount = CurrentFrame.Capacity * 5,
                                           .pName = "Renderer.IndirectDraws",
                                           .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                           .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                           .descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW | DESCRIPTOR_TYPE_INDIRECT_BUFFER };
        CurrentFrame.Opaque = Context.createBuffer(Arguments);
        CurrentFrame.Transparent = Context.createBuffer(Arguments);
        CurrentFrame.Shadow = Context.createBuffer(Arguments);
        CurrentFrame.Statistics = Context.createBuffer({ .Size = 16,
                                                         .elementCount = 4,
                                                         .pName = "Renderer.CullingStatistics",
                                                         .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                         .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                         .descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
        ASSERT(CurrentFrame.DrawData.isValid() && CurrentFrame.Opaque.isValid() && CurrentFrame.Transparent.isValid() &&
               CurrentFrame.Statistics.isValid());
    }
    Commands.setPipeline(ClearPipeline);
    Commands.setRootConstant({ CurrentFrame.Statistics.getUavIndex() }, 5);
    Commands.dispatch(1, 1, 1, { .storageBuffers = { &CurrentFrame.Statistics } });
    if (!CurrentFrame.Count)
    {
        return;
    }
    const bool Uploaded = Context.updateBuffer(CurrentFrame.DrawData, 0, DrawData.GetData(), uint64_t(CurrentFrame.Count) * sizeof(GpuDrawData));
    ASSERT(Uploaded);
    Commands.beginGpuTimestamp("GPU Frustum Culling");
    Commands.setPipeline(Pipeline);
    const CullingConstants Binding = { .frameIndex = FrameBuffer.getSrvIndex(),
                                       .drawsIndex = CurrentFrame.DrawData.getSrvIndex(),
                                       .instancesIndex = View.GpuScene.getBuffers().instances.getSrvIndex(),
                                       .opaqueIndex = CurrentFrame.Opaque.getUavIndex(),
                                       .transparentIndex = CurrentFrame.Transparent.getUavIndex(),
                                       .statisticsIndex = CurrentFrame.Statistics.getUavIndex(),
                                       .drawCount = CurrentFrame.Count,
                                       .cullingEnabled = Culling ? 1u : 0u,
                                       .shadowsIndex = CurrentFrame.Shadow.getUavIndex() };
    Commands.setRootConstants(Binding);
    Commands.dispatch((CurrentFrame.Count + 63) / 64, 1, 1,
                      { .buffers = { &FrameBuffer, &CurrentFrame.DrawData, &View.GpuScene.getBuffers().instances },
                        .storageBuffers = { &CurrentFrame.Opaque, &CurrentFrame.Transparent, &CurrentFrame.Statistics,
                                            &CurrentFrame.Shadow } });
    Commands.endGpuTimestamp();
}

hz::Dependencies GpuDrawList::GetDependencies(const hz::GPUBuffer& Frame, const SceneView& View,
                                             const hz::GPUBuffer* Indirect) const
{
    Buffers.Clear();
    Textures.Clear();
    Buffers.PushBack(&Frame);
    Buffers.PushBack(&GetDrawData(View.FrameIndex));
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.getBuffers();
    if (Gpu.instances.isValid())
    {
        Buffers.PushBack(&Gpu.instances);
    }
    if (Gpu.materials.isValid())
    {
        Buffers.PushBack(&Gpu.materials);
    }
    if (Gpu.textures.isValid())
    {
        Buffers.PushBack(&Gpu.textures);
    }
    const hz::GpuAssetTables& Tables = View.GpuScene.getAssetTables();
    const hz::GPUBuffer*      Previous = nullptr;
    for (const hz::GpuMeshData& MeshData : Tables.meshes)
    {
        if (!MeshData.id.isValid())
        {
            continue;
        }
        const hz::MeshAsset* Mesh = View.Assets.getMesh({ .index = MeshData.id.index, .generation = MeshData.id.generation });
        ASSERT(Mesh);
        if (&Mesh->getIndexBuffer() == Previous)
        {
            continue;
        }
        Previous = &Mesh->getIndexBuffer();
        Buffers.PushBack(Previous);
        for (uint32_t Index = 0; Index < Mesh->getVertexBufferCount(); ++Index)
        {
            Buffers.PushBack(&Mesh->getVertexBuffer(Index));
        }
    }
    for (const hz::GpuTextureData& TextureData : Tables.textures)
    {
        if (TextureData.id.isValid())
        {
            Textures.PushBack(
                &View.Assets.getTexture({ .index = TextureData.id.index, .generation = TextureData.id.generation })->getTexture());
        }
    }
    IndirectDependency = Indirect;
    return { .sampledTextures = { Textures.GetData(), Textures.Size() },
             .buffers = { Buffers.GetData(), Buffers.Size() },
             .indirectBuffers = { &IndirectDependency, Indirect ? 1u : 0u } };
}
