#include "GpuDrawList.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

GpuDrawList::GpuDrawList(hz::RenderContext& context): context(context)
{
    pipeline = context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain", .pName = "Renderer.FrustumCull" } },
                          .pFileName = "FrustumCulling.slang" },
          .pName = "Renderer.FrustumCull" });
    clearPipeline =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearStatistics" } },
                                                        .pFileName = "FrustumCulling.slang" },
                                        .pName = "Renderer.ClearCullingStatistics" });
    ASSERT(pipeline.isValid() && clearPipeline.isValid());
}

void GpuDrawList::prepare(hz::CommandList& commands, const hz::GPUBuffer& frameBuffer, const SceneView& view, bool culling)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuDrawList::prepare");
    const hz::GpuAssetTables& tables = view.gpuScene.getAssetTables();
    if (materialRevision != view.gpuScene.getAssetTables().revision)
    {
        materialIds.resize(tables.materials.count);
        uint32_t id = 0;
        for (uint32_t i = 0; i < tables.materials.count; ++i)
        {
            materialIds[i] = tables.materials.pData[i].id.isValid() ? id++ : UINT32_MAX;
        }
        materialRevision = view.gpuScene.getAssetTables().revision;
    }
    drawData.clear();
    geometryIndices.clear();
    uint32_t   geometryCount = 0;
    bool       geometryChanged = false;
    const auto markGeometryChanged = [&]
    {
        if (!geometryChanged)
        {
            for (uint32_t i = 0; i < geometries.size(); ++i)
            {
                geometries[i].previousIndex = i;
            }
            geometryChanged = true;
        }
    };
    for (uint32_t meshBatchIndex : view.scene.getActiveMeshBatches())
    {
        const hz::RenderMeshBatch&   draw = view.scene.getMeshBatches().pData[meshBatchIndex];
        const hz::RenderPrimitive&   primitive = view.scene.getPrimitives().pData[draw.primitive.index];
        const hz::GpuMeshData*       mesh = tables.getMesh(draw.mesh.mesh);
        const hz::GpuMaterialData*   material = tables.getMaterial(draw.material);
        if (!primitive.instanceCount || !mesh || !material)
        {
            continue;
        }
        ASSERT(mesh->vertexStreamCount == 3 && primitive.id == draw.primitive);
        const hz::GpuSubmeshData& submesh = tables.submeshes.pData[mesh->submeshOffset + draw.mesh.submeshIndex];
        uint32_t                  geometryIndex = UINT32_MAX;
        if (material->alphaMode != uint32_t(hz::MaterialAlphaMode::Blend))
        {
            const bool opaque = material->alphaMode == uint32_t(hz::MaterialAlphaMode::Opaque);
            geometryIndex = 0;
            while (geometryIndex < geometries.size() &&
                   (geometries[geometryIndex].mesh != draw.mesh || geometries[geometryIndex].opaque != opaque))
            {
                ++geometryIndex;
            }
            if (geometryIndex >= geometryCount)
            {
                if (geometryIndex != geometryCount || geometryIndex == geometries.size())
                {
                    markGeometryChanged();
                }
                if (geometryIndex == geometries.size())
                {
                    geometries.pushBack({ .mesh = draw.mesh, .opaque = opaque });
                }
                std::swap(geometries[geometryIndex], geometries[geometryCount]);
                geometryIndex = geometryCount++;
            }
        }
        for (uint32_t i = 0; i < primitive.instanceCount; ++i)
        {
            geometryIndices.pushBack(geometryIndex);
            drawData.pushBack({ .instance = view.scene.getInstanceIndices().pData[primitive.instanceOffset + i],
                                .material = draw.material.index,
                                .materialId = materialIds[draw.material.index],
                                .alphaMode = material->alphaMode,
                                .positions = tables.vertexStreams.pData[mesh->vertexStreamOffset].srv,
                                .normals = tables.vertexStreams.pData[mesh->vertexStreamOffset + 1].srv,
                                .texcoords = tables.vertexStreams.pData[mesh->vertexStreamOffset + 2].srv,
                                .indices = mesh->indexSrv,
                                .indexStride = mesh->indexStride,
                                .firstIndex = submesh.firstIndex,
                                .vertexOffset = submesh.vertexOffset,
                                .indexCount = submesh.indexCount });
        }
    }
    if (geometryCount != geometries.size())
    {
        markGeometryChanged();
        geometries.resize(geometryCount);
    }
    if (geometryChanged)
    {
        ++geometryRevision;
    }
    Frame& frame = frames[view.frameIndex];
    frame.count = drawData.size();
    if (frame.count > frame.capacity || !frame.statistics.isValid())
    {
        frame.capacity = TF_MAX(64u, TF_MAX(frame.count, frame.capacity * 2));
        frame.drawData = context.createBuffer({ .size = uint64_t(frame.capacity) * sizeof(GpuDrawData),
                                                .elementCount = frame.capacity * (uint32_t)(sizeof(GpuDrawData) / 4),
                                                .pName = "Renderer.DrawData",
                                                .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
                                                .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        const hz::BufferDesc arguments = { .size = uint64_t(frame.capacity) * 20,
                                           .elementCount = frame.capacity * 5,
                                           .pName = "Renderer.IndirectDraws",
                                           .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                           .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                           .descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW | DESCRIPTOR_TYPE_INDIRECT_BUFFER };
        frame.opaque = context.createBuffer(arguments);
        frame.transparent = context.createBuffer(arguments);
        frame.shadow = context.createBuffer(arguments);
        frame.statistics = context.createBuffer({ .size = 16,
                                                  .elementCount = 4,
                                                  .pName = "Renderer.CullingStatistics",
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
        ASSERT(frame.drawData.isValid() && frame.opaque.isValid() && frame.transparent.isValid() && frame.statistics.isValid());
    }
    commands.setPipeline(clearPipeline);
    commands.setRootConstant({ frame.statistics.getUavIndex() }, 5);
    commands.dispatch(1, 1, 1, { .storageBuffers = { &frame.statistics } });
    if (!frame.count)
    {
        return;
    }
    const bool uploaded = context.updateBuffer(frame.drawData, 0, drawData.data(), uint64_t(frame.count) * sizeof(GpuDrawData));
    ASSERT(uploaded);
    commands.beginGpuTimestamp("GPU Frustum Culling");
    commands.setPipeline(pipeline);
    commands.setRootConstant({ frameBuffer.getSrvIndex(), frame.drawData.getSrvIndex(), view.gpuScene.getBuffers().instances.getSrvIndex(),
                               frame.opaque.getUavIndex(), frame.transparent.getUavIndex(), frame.statistics.getUavIndex(), frame.count,
                               culling ? 1u : 0u, frame.shadow.getUavIndex() });
    commands.dispatch((frame.count + 63) / 64, 1, 1,
                      { .buffers = { &frameBuffer, &frame.drawData, &view.gpuScene.getBuffers().instances },
                        .storageBuffers = { &frame.opaque, &frame.transparent, &frame.statistics, &frame.shadow } });
    commands.endGpuTimestamp();
}

hz::Dependencies GpuDrawList::getDependencies(const hz::GPUBuffer& frame, const SceneView& view, const hz::GPUBuffer* indirect) const
{
    buffers.clear();
    textures.clear();
    buffers.pushBack(&frame);
    buffers.pushBack(&getDrawData(view.frameIndex));
    const hz::GpuSceneBuffers& gpu = view.gpuScene.getBuffers();
    if (gpu.instances.isValid())
    {
        buffers.pushBack(&gpu.instances);
    }
    if (gpu.materials.isValid())
    {
        buffers.pushBack(&gpu.materials);
    }
    if (gpu.textures.isValid())
    {
        buffers.pushBack(&gpu.textures);
    }
    const hz::GpuAssetTables&       tables = view.gpuScene.getAssetTables();
    const hz::GPUBuffer*            previous = nullptr;
    for (const hz::GpuMeshData& meshData : tables.meshes)
    {
        if (!meshData.id.isValid())
        {
            continue;
        }
        const hz::MeshAsset* mesh = view.assets.getMesh({ .index = meshData.id.index, .generation = meshData.id.generation });
        ASSERT(mesh);
        if (&mesh->getIndexBuffer() == previous)
        {
            continue;
        }
        previous = &mesh->getIndexBuffer();
        buffers.pushBack(previous);
        for (uint32_t i = 0; i < mesh->getVertexBufferCount(); ++i)
        {
            buffers.pushBack(&mesh->getVertexBuffer(i));
        }
    }
    for (const hz::GpuTextureData& textureData : tables.textures)
    {
        if (textureData.id.isValid())
        {
            textures.pushBack(
                &view.assets.getTexture({ .index = textureData.id.index, .generation = textureData.id.generation })->getTexture());
        }
    }
    indirectDependency = indirect;
    return { .sampledTextures = { textures.data(), textures.size() },
             .buffers = { buffers.data(), buffers.size() },
             .indirectBuffers = { &indirectDependency, indirect ? 1u : 0u } };
}
