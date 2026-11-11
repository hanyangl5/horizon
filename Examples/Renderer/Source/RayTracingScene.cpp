#include "RayTracingScene.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

void RayTracingScene::update(hz::CommandList& commands, const SceneView& view)
{
    ASSERT(view.frameIndex < frames.size());
    for (const hz::unique_ptr<BLAS>& blas : bottomLevels)
    {
        blas->usedByFrame[view.frameIndex] = false;
    }
    instances.clear();
    buffers.clear();
    textures.clear();

    collectInstances(commands, view);
    updateTLAS(commands, frames[view.frameIndex]);
    collectTextures(view);
    removeUnusedBLAS();
}

RayTracingScene::BLAS& RayTracingScene::getBLAS(hz::CommandList& commands, hz::GpuSubmeshRef meshRef, const hz::MeshAsset& mesh,
                                                bool opaque)
{
    for (const hz::unique_ptr<BLAS>& blas : bottomLevels)
    {
        if (blas->mesh == meshRef && blas->opaque == opaque)
        {
            return *blas;
        }
    }

    const IndirectDrawIndexArguments& submesh = mesh.getDraw(meshRef.submeshIndex);
    const hz::GPUBuffer&              vertices = mesh.getVertexBuffer(0);
    const uint32_t                    stride = mesh.getVertexStride(0);
    const int32_t                     baseVertex = int32_t(submesh.vertexOffset);
    // Cooked GeometryTF uses nonnegative base vertices and triangle lists.
    ASSERT(baseVertex >= 0 && stride == 12 && submesh.indexCount % 3 == 0);
    const uint64_t vertexOffset = uint64_t(baseVertex) * stride;
    ASSERT(vertices.getSize() >= vertexOffset && (vertices.getSize() - vertexOffset) / stride <= UINT32_MAX);
    const hz::AccelerationStructureGeometry geometry = {
        .pVertexBuffer = &vertices,
        .pIndexBuffer = &mesh.getIndexBuffer(),
        .vertexOffset = vertexOffset,
        .indexOffset = uint64_t(submesh.startIndex) * (mesh.getIndexType() == INDEX_TYPE_UINT16 ? 2u : 4u),
        .vertexCount = uint32_t((vertices.getSize() - vertexOffset) / stride),
        .vertexStride = stride,
        .indexCount = submesh.indexCount,
        .indexType = mesh.getIndexType(),
        .opaque = opaque,
    };
    hz::unique_ptr<BLAS> blas = hz::make_unique<BLAS>();
    blas->mesh = meshRef;
    blas->opaque = opaque;
    blas->structure = context.createAccelerationStructure({ .geometries = { &geometry, 1 } });
    ASSERT(blas->structure.isValid());
    commands.buildAccelerationStructure(blas->structure);
    BLAS& result = *blas;
    bottomLevels.pushBack(std::move(blas));
    return result;
}

void RayTracingScene::collectInstances(hz::CommandList& commands, const SceneView& view)
{
    const hz::GpuAssetTables& tables = view.gpuScene.getAssetTables();
    uint32_t                  drawIndex = 0;
    for (uint32_t meshBatchIndex : view.scene.getActiveMeshBatches())
    {
        const hz::RenderMeshBatch& draw = view.scene.getMeshBatches().pData[meshBatchIndex];
        const hz::RenderPrimitive& primitive = view.scene.getPrimitives().pData[draw.primitive.index];
        const hz::GpuMeshData*     meshData = tables.getMesh(draw.mesh.mesh);
        const hz::GpuMaterialData* material = tables.getMaterial(draw.material);
        if (!primitive.instanceCount || !meshData || !material)
        {
            continue;
        }
        // Keep instance IDs aligned with GpuDrawList, including its blended draws.
        if (material->alphaMode == uint32_t(hz::MaterialAlphaMode::Blend))
        {
            drawIndex += primitive.instanceCount;
            continue;
        }

        const hz::MeshAsset* mesh = view.assets.getMesh({ draw.mesh.mesh.index, draw.mesh.mesh.generation });
        ASSERT(mesh && meshData->vertexStreamCount == 3);
        BLAS& blas = getBLAS(commands, draw.mesh, *mesh, material->alphaMode == uint32_t(hz::MaterialAlphaMode::Opaque));
        blas.usedByFrame[view.frameIndex] = true;
        for (uint32_t stream = 0; stream < mesh->getVertexBufferCount(); ++stream)
        {
            addBuffer(&mesh->getVertexBuffer(stream));
        }
        addBuffer(&mesh->getIndexBuffer());

        for (uint32_t i = 0; i < primitive.instanceCount; ++i, ++drawIndex)
        {
            const uint32_t                    instanceIndex = view.scene.getInstanceIndices().pData[primitive.instanceOffset + i];
            const Matrix4&                    transform = view.scene.getInstances().pData[instanceIndex].current;
            hz::AccelerationStructureInstance instance = {
                .pBottom = &blas.structure,
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
            instances.pushBack(instance);
        }
    }
}

bool RayTracingScene::Frame::haveInstancesChanged(hz::Span<hz::AccelerationStructureInstance> current) const
{
    if (instances.size() != current.count || (current.count && !tlas.isValid()))
    {
        return true;
    }
    for (uint32_t i = 0; i < current.count; ++i)
    {
        const hz::AccelerationStructureInstance& previous = instances[i];
        const hz::AccelerationStructureInstance& next = current.pData[i];
        if (previous.pBottom != next.pBottom || previous.instanceId != next.instanceId || previous.mask != next.mask ||
            previous.hitGroupOffset != next.hitGroupOffset || previous.flags != next.flags)
        {
            return true;
        }
        for (uint32_t element = 0; element < 12; ++element)
        {
            if (previous.transform[element] != next.transform[element])
            {
                return true;
            }
        }
    }
    return false;
}

void RayTracingScene::updateTLAS(hz::CommandList& commands, Frame& frame)
{
    if (!frame.haveInstancesChanged({ instances.data(), instances.size() }))
    {
        return;
    }
    if (instances.empty())
    {
        frame.tlas = {};
        frame.instances.clear();
        return;
    }

    const bool update = frame.tlas.isValid() && frame.instances.size() == instances.size();
    if (!update)
    {
        frame.tlas = context.createAccelerationStructure({
            .type = ACCELERATION_STRUCTURE_TYPE_TOP,
            .flags = AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                                                     ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE),
            .instances = { instances.data(), instances.size() },
        });
    }
    ASSERT(frame.tlas.isValid());
    commands.buildAccelerationStructure(frame.tlas, { instances.data(), instances.size() }, update);
    // Reuse both allocations when staging the next frame.
    std::swap(frame.instances, instances);
}

void RayTracingScene::collectTextures(const SceneView& view)
{
    for (const hz::GpuTextureData& texture : view.gpuScene.getAssetTables().textures)
    {
        if (texture.id.isValid())
        {
            const hz::TextureAsset* asset = view.assets.getTexture({ texture.id.index, texture.id.generation });
            ASSERT(asset);
            textures.pushBack(&asset->getTexture());
        }
    }
}

void RayTracingScene::addBuffer(const hz::GPUBuffer* buffer)
{
    for (const hz::GPUBuffer* existing : buffers)
    {
        if (existing == buffer)
        {
            return;
        }
    }
    buffers.pushBack(buffer);
}

void RayTracingScene::removeUnusedBLAS()
{
    // Clear the completed frame's references first, then retain BLAS used by any other frame in flight.
    for (uint32_t i = 0; i < bottomLevels.size();)
    {
        bool inUse = false;
        for (bool used : bottomLevels[i]->usedByFrame)
        {
            inUse |= used;
        }
        if (inUse)
        {
            ++i;
            continue;
        }
        if (i + 1 != bottomLevels.size())
        {
            bottomLevels[i] = std::move(bottomLevels[bottomLevels.size() - 1]);
        }
        bottomLevels.popBack();
    }
}
