#include "RayTracingScene.h"
#include "GpuDrawList.h"
#include "Scene/MeshAsset.h"

void RayTracingScene::Update(hz::CommandList& Commands, const SceneView& View, const GpuDrawList& Draws)
{
    const hz::Span<DrawGeometry> Geometries = Draws.GetGeometries();
    if (GeometryRevision != Draws.GetGeometryRevision())
    {
        hz::Array<hz::GPUAccelerationStructure> Previous = std::move(BottomLevelAS);
        BottomLevelAS.resize(Geometries.Count);
        for (uint32_t Index = 0; Index < Geometries.Count; ++Index)
        {
            const DrawGeometry& Source = Geometries.Data[Index];
            if (GeometryRevision + 1 == Draws.GetGeometryRevision() && Source.PreviousIndex < Previous.Size())
            {
                BottomLevelAS[Index] = std::move(Previous[Source.PreviousIndex]);
                continue;
            }
            const hz::MeshAsset* Mesh = View.Assets.getMesh({ Source.Mesh.mesh.index, Source.Mesh.mesh.generation });
            ASSERT(Mesh);
            const IndirectDrawIndexArguments& Submesh = Mesh->getDraw(Source.Mesh.submeshIndex);
            const hz::GPUBuffer&              Vertices = Mesh->getVertexBuffer(0);
            const uint32_t                    Stride = Mesh->getVertexStride(0);
            const int32_t                     BaseVertex = int32_t(Submesh.vertexOffset);
            ASSERT(BaseVertex >= 0 && Stride == 12 && Submesh.indexCount % 3 == 0);
            const uint64_t VertexOffset = uint64_t(BaseVertex) * Stride;
            ASSERT(Vertices.getSize() >= VertexOffset);
            const hz::AccelerationStructureGeometry Geometry = {
                .pVertexBuffer = &Vertices,
                .pIndexBuffer = &Mesh->getIndexBuffer(),
                .vertexOffset = VertexOffset,
                .indexOffset = uint64_t(Submesh.startIndex) * (Mesh->getIndexType() == IndexTypeUInt16 ? 2u : 4u),
                .vertexCount = uint32_t((Vertices.getSize() - VertexOffset) / Stride),
                .vertexStride = Stride,
                .indexCount = Submesh.indexCount,
                .indexType = Mesh->getIndexType(),
                .opaque = Source.Opaque,
            };
            BottomLevelAS[Index] = Context.createAccelerationStructure({ .geometries = { &Geometry, 1 } });
            ASSERT(BottomLevelAS[Index].IsValid());
            Commands.buildAccelerationStructure(BottomLevelAS[Index]);
        }
        GeometryRevision = Draws.GetGeometryRevision();
    }

    const hz::Span<GpuDrawData> Prepared = Draws.GetDraws();
    uint32_t                    InstanceCount = 0;
    for (const GpuDrawData& Draw : Prepared)
    {
        InstanceCount += Draw.alphaMode != uint32_t(hz::MaterialAlphaMode::Blend);
    }
    if (!InstanceCount)
    {
        TopLevelAS = {};
        return;
    }
    if (TopLevelAS.getInstanceCount() != InstanceCount)
    {
        TopLevelAS = Context.createAccelerationStructure({
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
        const GpuDrawData& Draw = Prepared.Data[DrawIndex];
        if (Draw.alphaMode == uint32_t(hz::MaterialAlphaMode::Blend))
        {
            continue;
        }
        const Matrix4&                    Transform = View.Scene.getInstances().Data[Draw.instance].current;
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
        TopLevelAS.setInstance(InstanceIndex++, Instance);
    }
    Commands.updateAccelerationStructure(TopLevelAS);
}
