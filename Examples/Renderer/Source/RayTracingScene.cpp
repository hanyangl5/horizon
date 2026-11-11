#include "RayTracingScene.h"
#include "GpuDrawList.h"
#include "Scene/MeshAsset.h"

void RayTracingScene::update(hz::CommandList& commands, const SceneView& view, const GpuDrawList& draws)
{
    const hz::Span<DrawGeometry> geometries = draws.getGeometries();
    if (geometryRevision != draws.getGeometryRevision())
    {
        hz::Array<hz::GPUAccelerationStructure> previous = std::move(blas);
        blas.resize(geometries.count);
        for (uint32_t i = 0; i < geometries.count; ++i)
        {
            const DrawGeometry& source = geometries.pData[i];
            if (geometryRevision + 1 == draws.getGeometryRevision() && source.previousIndex < previous.size())
            {
                blas[i] = std::move(previous[source.previousIndex]);
                continue;
            }
            const hz::MeshAsset* mesh = view.assets.getMesh({ source.mesh.mesh.index, source.mesh.mesh.generation });
            ASSERT(mesh);
            const IndirectDrawIndexArguments& submesh = mesh->getDraw(source.mesh.submeshIndex);
            const hz::GPUBuffer&              vertices = mesh->getVertexBuffer(0);
            const uint32_t                    stride = mesh->getVertexStride(0);
            const int32_t                     baseVertex = int32_t(submesh.vertexOffset);
            ASSERT(baseVertex >= 0 && stride == 12 && submesh.indexCount % 3 == 0);
            const uint64_t vertexOffset = uint64_t(baseVertex) * stride;
            ASSERT(vertices.getSize() >= vertexOffset);
            const hz::AccelerationStructureGeometry geometry = {
                .pVertexBuffer = &vertices,
                .pIndexBuffer = &mesh->getIndexBuffer(),
                .vertexOffset = vertexOffset,
                .indexOffset = uint64_t(submesh.startIndex) * (mesh->getIndexType() == INDEX_TYPE_UINT16 ? 2u : 4u),
                .vertexCount = uint32_t((vertices.getSize() - vertexOffset) / stride),
                .vertexStride = stride,
                .indexCount = submesh.indexCount,
                .indexType = mesh->getIndexType(),
                .opaque = source.opaque,
            };
            blas[i] = context.createAccelerationStructure({ .geometries = { &geometry, 1 } });
            ASSERT(blas[i].isValid());
            commands.buildAccelerationStructure(blas[i]);
        }
        geometryRevision = draws.getGeometryRevision();
    }

    const hz::Span<GpuDrawData> prepared = draws.getDraws();
    uint32_t                    instanceCount = 0;
    for (const GpuDrawData& draw : prepared)
    {
        instanceCount += draw.alphaMode != uint32_t(hz::MaterialAlphaMode::Blend);
    }
    if (!instanceCount)
    {
        tlas = {};
        return;
    }
    if (tlas.getInstanceCount() != instanceCount)
    {
        tlas = context.createAccelerationStructure({
            .type = ACCELERATION_STRUCTURE_TYPE_TOP,
            .flags = AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                                                     ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE),
            .instanceCount = instanceCount,
        });
    }
    ASSERT(tlas.isValid());
    uint32_t instanceIndex = 0;
    for (uint32_t drawIndex = 0; drawIndex < prepared.count; ++drawIndex)
    {
        const GpuDrawData& draw = prepared.pData[drawIndex];
        if (draw.alphaMode == uint32_t(hz::MaterialAlphaMode::Blend))
        {
            continue;
        }
        const Matrix4&                    transform = view.scene.getInstances().pData[draw.instance].current;
        hz::AccelerationStructureInstance instance = {
            .pBottom = &blas[draws.getGeometryIndex(drawIndex)],
            .instanceId = drawIndex,
            .flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE,
        };
        for (uint32_t row = 0; row < 3; ++row)
        {
            for (uint32_t column = 0; column < 4; ++column)
            {
                instance.transform[row * 4 + column] = transform.getCol(column).getElem(row);
            }
        }
        tlas.setInstance(instanceIndex++, instance);
    }
    commands.updateAccelerationStructure(tlas);
}
