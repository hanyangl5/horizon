#include "GpuDrawList.h"
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

void GpuDrawList::prepare(hz::CommandList& commands, const hz::GPUBuffer& frameBuffer, const hz::SceneFrameSnapshot& scene, bool culling)
{
    const hz::GpuAssetTableSnapshot tables = scene.getAssets().getGpuTables();
    if (materialRevision != scene.getAssets().getRevision())
    {
        materialIds.resize(tables.materials.count);
        uint32_t id = 0;
        for (uint32_t i = 0; i < tables.materials.count; ++i)
        {
            materialIds[i] = tables.materials.pData[i].id.isValid() ? id++ : UINT32_MAX;
        }
        materialRevision = scene.getAssets().getRevision();
    }
    records.clear();
    for (uint32_t slot : scene.getActiveDraws())
    {
        const hz::RenderDraw&        draw = scene.getDraws().pData[slot];
        const hz::RenderObject&      object = scene.getObjects().pData[draw.object.index];
        const hz::GpuMeshRecord*     mesh = tables.getMesh(draw.mesh.mesh);
        const hz::GpuMaterialRecord* material = tables.getMaterial(draw.material);
        if (!object.instanceCount || !mesh || !material)
        {
            continue;
        }
        ASSERT(mesh->vertexStreamCount == 3 && object.id == draw.object);
        const hz::GpuSubmeshRecord& submesh = tables.submeshes.pData[mesh->submeshOffset + draw.mesh.submeshIndex];
        for (uint32_t i = 0; i < object.instanceCount; ++i)
        {
            records.pushBack({ .instance = scene.getInstanceIndices().pData[object.instanceOffset + i],
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
    Frame& frame = frames[scene.getBackingIndex()];
    frame.count = records.size();
    if (frame.count > frame.capacity || !frame.statistics.isValid())
    {
        frame.capacity = TF_MAX(64u, TF_MAX(frame.count, frame.capacity * 2));
        frame.records = context.createBuffer({ .size = uint64_t(frame.capacity) * sizeof(GpuDrawRecord),
                                               .elementCount = frame.capacity * (uint32_t)(sizeof(GpuDrawRecord) / 4),
                                               .pName = "Renderer.DrawRecords",
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
        ASSERT(frame.records.isValid() && frame.opaque.isValid() && frame.transparent.isValid() && frame.statistics.isValid());
    }
    commands.setPipeline(clearPipeline);
    commands.setRootConstant({ frame.statistics.getUavIndex() }, 5);
    commands.dispatch(1, 1, 1, { .storageBuffers = { &frame.statistics } });
    if (!frame.count)
    {
        return;
    }
    const bool uploaded = context.updateBuffer(frame.records, 0, records.data(), uint64_t(frame.count) * sizeof(GpuDrawRecord));
    ASSERT(uploaded);
    commands.beginGpuTimestamp("GPU Frustum Culling");
    commands.setPipeline(pipeline);
    commands.setRootConstant({ frameBuffer.getSrvIndex(), frame.records.getSrvIndex(), scene.getBuffers().instances.getSrvIndex(),
                               frame.opaque.getUavIndex(), frame.transparent.getUavIndex(), frame.statistics.getUavIndex(), frame.count,
                               culling ? 1u : 0u, frame.shadow.getUavIndex() });
    commands.dispatch((frame.count + 63) / 64, 1, 1,
                      { .buffers = { &frameBuffer, &frame.records, &scene.getBuffers().instances },
                        .storageBuffers = { &frame.opaque, &frame.transparent, &frame.statistics, &frame.shadow } });
    commands.endGpuTimestamp();
}

hz::Dependencies GpuDrawList::getDependencies(const hz::GPUBuffer& frame, const hz::SceneFrameSnapshot& scene,
                                              const hz::GPUBuffer& indirect) const
{
    buffers.clear();
    textures.clear();
    buffers.pushBack(&frame);
    buffers.pushBack(&getRecords(scene.getBackingIndex()));
    const hz::SceneFrameBuffers& gpu = scene.getBuffers();
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
    const hz::GpuAssetTableSnapshot tables = scene.getAssets().getGpuTables();
    const hz::GPUBuffer*            previous = nullptr;
    for (const hz::GpuMeshRecord& record : tables.meshes)
    {
        if (!record.id.isValid())
        {
            continue;
        }
        const hz::MeshAsset* mesh = scene.getAssets().getMesh({ .index = record.id.index, .generation = record.id.generation });
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
    for (const hz::GpuTextureRecord& record : tables.textures)
    {
        if (record.id.isValid())
        {
            textures.pushBack(
                &scene.getAssets().getTexture({ .index = record.id.index, .generation = record.id.generation })->getTexture());
        }
    }
    indirectDependency = &indirect;
    return { .sampledTextures = { textures.data(), textures.size() },
             .buffers = { buffers.data(), buffers.size() },
             .indirectBuffers = { &indirectDependency, 1 } };
}
