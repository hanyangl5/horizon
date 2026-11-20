#include "RayTracingScene.h"
#include "Scene/GpuDrawList.h"
#include "Scene/MeshAsset.h"
#include "Scene/GpuSkinning.h"

void RayTracingScene::Update(hz::CommandList& Commands, const hz::SceneView& View, const hz::GpuDrawList& Draws)
{
    const hz::Span<hz::DrawGeometry> Geometries = Draws.GetGeometries();
    // Bottom levels built this frame already hold this frame's skinned positions.
    const bool                       Rebuilt = GeometryRevision != Draws.GetGeometryRevision();
    if (Rebuilt)
    {
        hz::Array<hz::GPUAccelerationStructure> Previous = std::move(BottomLevelAS);
        BottomLevelAS.Resize(Geometries.Count);
        SkinnedBottomLevels.Clear();
        Reused.Clear();
        for (uint32_t Index = 0; Index < Geometries.Count; ++Index)
        {
            const hz::DrawGeometry& Source = Geometries.Data[Index];
            if (Source.SkinnedInstance.IsValid())
            {
                SkinnedBottomLevels.PushBack(Index);
                Reused.PushBack(GeometryRevision + 1 == Draws.GetGeometryRevision() && Source.PreviousIndex < Previous.Size());
            }
            if (GeometryRevision + 1 == Draws.GetGeometryRevision() && Source.PreviousIndex < Previous.Size())
            {
                BottomLevelAS[Index] = std::move(Previous[Source.PreviousIndex]);
                continue;
            }
            const hz::MeshAsset* Mesh = View.Assets.GetMesh({ Source.Mesh.mesh.index, Source.Mesh.mesh.generation });
            ASSERT(Mesh);
            const IndirectDrawIndexArguments& Submesh = Mesh->GetDraw(Source.Mesh.submeshIndex);
            // A skinned instance traces its own skinned positions, indexed like the mesh streams.
            const hz::GpuSkinnedVertices* Skinned =
                Source.SkinnedInstance.IsValid() && View.Skinning ? View.Skinning->Find(Source.SkinnedInstance) : nullptr;
            ASSERT(Skinned || !Source.SkinnedInstance.IsValid());
            const hz::GPUBuffer& Vertices = Skinned ? Skinned->positions : Mesh->GetVertexBuffer(0);
            const uint32_t       Stride = Mesh->GetVertexStride(0);
            const int32_t        BaseVertex = int32_t(Submesh.vertexOffset);
            ASSERT(BaseVertex >= 0 && Stride == 12 && Submesh.indexCount % 3 == 0);
            const uint64_t VertexOffset = uint64_t(BaseVertex) * Stride;
            ASSERT(Vertices.GetSize() >= VertexOffset);
            const hz::AccelerationStructureGeometry Geometry = {
                .pVertexBuffer = &Vertices,
                .pIndexBuffer = &Mesh->GetIndexBuffer(),
                .vertexOffset = VertexOffset,
                .indexOffset = uint64_t(Submesh.startIndex) * (Mesh->GetIndexType() == INDEX_TYPE_UINT16 ? 2u : 4u),
                .vertexCount = uint32_t((Vertices.GetSize() - VertexOffset) / Stride),
                .vertexStride = Stride,
                .indexCount = Submesh.indexCount,
                .indexType = Mesh->GetIndexType(),
                .opaque = Source.Opaque,
            };
            BottomLevelAS[Index] = Context.CreateAccelerationStructure(
                { .Flags = Skinned ? AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                                                                          ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
                                   : ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
                  .geometries = { &Geometry, 1 } });
            ASSERT(BottomLevelAS[Index].IsValid());
            Commands.BuildAccelerationStructure(BottomLevelAS[Index]);
        }
        GeometryRevision = Draws.GetGeometryRevision();
    }
    for (uint32_t Skinned = 0; Skinned < SkinnedBottomLevels.Size(); ++Skinned)
    {
        if (!Rebuilt || Reused[Skinned])
        {
            Commands.BuildAccelerationStructure(BottomLevelAS[SkinnedBottomLevels[Skinned]], {}, true);
        }
    }

    const hz::Span<hz::GpuDrawData> Prepared = Draws.GetDraws();
    uint32_t                    InstanceCount = 0;
    for (const hz::GpuDrawData& Draw : Prepared)
    {
        InstanceCount += Draw.alphaMode != uint32_t(hz::MaterialAlphaMode::Blend);
    }
    if (!InstanceCount)
    {
        TopLevelAS = {};
        return;
    }
    if (TopLevelAS.GetInstanceCount() != InstanceCount)
    {
        TopLevelAS = Context.CreateAccelerationStructure({
            .Type = ACCELERATION_STRUCTURE_TYPE_TOP,
            .Flags = AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                                                     ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE),
            .instanceCount = InstanceCount,
        });
    }
    ASSERT(TopLevelAS.IsValid());
    uint32_t InstanceIndex = 0;
    for (uint32_t DrawIndex = 0; DrawIndex < Prepared.Count; ++DrawIndex)
    {
        const hz::GpuDrawData& Draw = Prepared.Data[DrawIndex];
        if (Draw.alphaMode == uint32_t(hz::MaterialAlphaMode::Blend))
        {
            continue;
        }
        const Matrix4&                    Transform = View.Scene.GetInstances().Data[Draw.instance].current;
        hz::AccelerationStructureInstance Instance = {
            .pBottom = &BottomLevelAS[Draws.GetGeometryIndex(DrawIndex)],
            .instanceId = DrawIndex,
            .Flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE,
        };
        for (uint32_t Row = 0; Row < 3; ++Row)
        {
            for (uint32_t Column = 0; Column < 4; ++Column)
            {
                Instance.transform[Row * 4 + Column] = Transform.getCol(Column).getElem(Row);
            }
        }
        TopLevelAS.SetInstance(InstanceIndex++, Instance);
    }
    Commands.UpdateAccelerationStructure(TopLevelAS);
}
