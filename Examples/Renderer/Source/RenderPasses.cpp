#include "RenderPasses.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Core/ILog.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

constexpr hz::Format kDepthFormat = hz::Format::D24_UNORM_S8_UINT;
constexpr hz::Format kGBufferFormats[] = {
    hz::Format::B10G11R11_UFLOAT,
    hz::Format::R10G10B10A2_UNORM,
    hz::Format::R8G8B8A8_UNORM,
    hz::Format::R8G8B8A8_UNORM,
};
constexpr const char* kGBufferNames[] = {
    "Renderer.GBuffer.Emissive",
    "Renderer.GBuffer.NormalMaterial",
    "Renderer.GBuffer.BaseColorMetallic",
    "Renderer.GBuffer.MotionMaterialId",
};

struct PostProcessRootConstants
{
    uint32_t sceneColorIndex;
    uint32_t depthIndex;
    uint32_t outputMode;
    float    paperWhiteNits;
    float    peakNits;
};

// ByteAddressBuffer offsets in Geometry.slang and Lighting.slang.
static_assert(sizeof(hz::RenderInstance) == 192 && offsetof(hz::RenderInstance, current) == 32 &&
              offsetof(hz::RenderInstance, previous) == 96);
static_assert(sizeof(hz::GpuMaterialRecord) == 88 && offsetof(hz::GpuMaterialRecord, baseColorFactor) == 40 &&
              offsetof(hz::GpuMaterialRecord, alphaMode) == 80);
static_assert(sizeof(hz::GpuTextureRecord) == 12);
static_assert(sizeof(hz::RenderLight) == 192 && offsetof(hz::RenderLight, transform) == 32 && offsetof(hz::RenderLight, type) == 96 &&
              offsetof(hz::RenderLight, ready) == 184);

GBuffer::GBuffer(hz::RenderContext& context): context(context)
{
    pipeline = context.createGraphicsPipeline({
    .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.GeometryVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.GeometryPS" },
        },
        .pFileName = "Geometry.slang",
    },
    .depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
    .colorTargets = {
        { .format = kGBufferFormats[0] },
        { .format = kGBufferFormats[1] },
        { .format = kGBufferFormats[2] },
        { .format = kGBufferFormats[3] },
    },
    .depthStencilFormat = kDepthFormat,
    .pName = "Renderer.GeometryPipeline",
});

    sampler = context.createSampler();
    ASSERT(pipeline.isValid() && sampler.isValid());
}

void GBuffer::load(uint32_t width, uint32_t height)
{
    hz::TextureDesc targetDesc = {
        .width = width,
        .height = height,
        .startState = RESOURCE_STATE_RENDER_TARGET,
        .descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .renderTarget = true,
    };
    for (uint32_t i = 0; i < GBuffer::gbufferCount; ++i)
    {
        targetDesc.format = kGBufferFormats[i];
        targetDesc.pName = kGBufferNames[i];
        gbuffer[i] = context.createTexture(targetDesc);
    }

    targetDesc.format = kDepthFormat;
    targetDesc.startState = RESOURCE_STATE_DEPTH_WRITE;
    targetDesc.pName = "Renderer.Depth";
    depth = context.createTexture(targetDesc);
}

void GBuffer::unload()
{
    for (hz::GPUTexture& target : gbuffer)
        target = {};
    depth = {};
}

void GBuffer::update() {}

void GBuffer::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::SceneFrameSnapshot& scene)
{
    const hz::SceneFrameBuffers&    gpu = scene.getBuffers();
    const hz::GpuAssetTableSnapshot tables = scene.getAssets().getGpuTables();
    if (materialRevision != scene.getAssets().getRevision())
    {
        materialIds.resize(tables.materials.count);
        uint32_t id = 0;
        for (uint32_t i = 0; i < tables.materials.count; ++i)
            materialIds[i] = tables.materials.pData[i].id.isValid() ? id++ : UINT32_MAX;
        materialRevision = scene.getAssets().getRevision();
    }
    buffers.clear();
    textures.clear();
    buffers.pushBack(&frame);
    if (gpu.instances.isValid())
        buffers.pushBack(&gpu.instances);
    if (gpu.materials.isValid())
        buffers.pushBack(&gpu.materials);
    if (gpu.textures.isValid())
        buffers.pushBack(&gpu.textures);
    const hz::GPUBuffer* previousIndex = nullptr;
    for (const hz::GpuMeshRecord& record : tables.meshes)
    {
        if (!record.id.isValid())
            continue;
        const hz::MeshAsset* mesh = scene.getAssets().getMesh({ .index = record.id.index, .generation = record.id.generation });
        ASSERT(mesh && mesh->getVertexBufferCount() == 3);
        if (&mesh->getIndexBuffer() == previousIndex)
            continue;
        previousIndex = &mesh->getIndexBuffer();
        buffers.pushBack(previousIndex);
        for (uint32_t i = 0; i < mesh->getVertexBufferCount(); ++i)
            buffers.pushBack(&mesh->getVertexBuffer(i));
    }
    for (const hz::GpuTextureRecord& record : tables.textures)
        if (record.id.isValid())
            textures.pushBack(
                &scene.getAssets().getTexture({ .index = record.id.index, .generation = record.id.generation })->getTexture());
    const ClearValue black = {};
    commands.beginGpuTimestamp("Geometry");
    commands.beginRendering({
        .colorAttachments =  {
            { .pTexture = &gbuffer[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            { .pTexture = &gbuffer[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            { .pTexture = &gbuffer[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            { .pTexture = &gbuffer[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
        } ,
        .depthAttachment = { .pTexture = &depth,
                             .loadAction = LOAD_ACTION_CLEAR,
                             .storeAction = STORE_ACTION_STORE,
                             .clearValue = { .depth = 1.0f } },
    }, {
        .sampledTextures = { textures.data(), textures.size() },
        .buffers = { buffers.data(), buffers.size() },
    });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    commands.setRootConstant({ frame.getSrvIndex(), gpu.instances.isValid() ? gpu.instances.getSrvIndex() : UINT32_MAX,
                               gpu.materials.isValid() ? gpu.materials.getSrvIndex() : UINT32_MAX,
                               gpu.textures.isValid() ? gpu.textures.getSrvIndex() : UINT32_MAX, sampler.getIndex() },
                             3);

    const hz::MeshAsset* previousMesh = nullptr;
    for (uint32_t index : scene.getActiveDraws())
    {
        const hz::RenderDraw&   draw = scene.getDraws().pData[index];
        const hz::RenderObject& object = scene.getObjects().pData[draw.object.index];
        ASSERT(object.id == draw.object);
        if (!object.instanceCount)
            continue;
        const hz::GpuMeshRecord* record = tables.getMesh(draw.mesh.mesh);
        ASSERT(record && tables.getMaterial(draw.material));
        const hz::MeshAsset* mesh = scene.getAssets().getMesh({ .index = draw.mesh.mesh.index, .generation = draw.mesh.mesh.generation });
        if (mesh != previousMesh)
        {
            commands.setIndexBuffer(mesh->getIndexBuffer(), 0, mesh->getIndexType());
            commands.setRootConstant(
                { mesh->getVertexBuffer(0).getSrvIndex(), mesh->getVertexBuffer(1).getSrvIndex(), mesh->getVertexBuffer(2).getSrvIndex() },
                8);
            previousMesh = mesh;
        }
        const hz::GpuSubmeshRecord& submesh = tables.submeshes.pData[record->submeshOffset + draw.mesh.submeshIndex];
        for (uint32_t i = 0; i < object.instanceCount; ++i)
        {
            const uint32_t instance = scene.getInstanceIndices().pData[object.instanceOffset + i];
            commands.setRootConstant({ instance, draw.material.index, materialIds[draw.material.index] });
            commands.drawIndexed(submesh.indexCount, submesh.firstIndex, submesh.vertexOffset);
        }
    }
    commands.endRendering();
    commands.endGpuTimestamp();
}

Lighting::Lighting(hz::RenderContext& context): context(context)
{
    pipeline = context.createGraphicsPipeline({
        .shaderDesc={
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.LightingVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.LightingPS" },
        },
        .pFileName = "Lighting.slang",
    },
        .colorTargets = { { .format = hz::Format::B10G11R11_UFLOAT } },
        .pName = "Renderer.LightingPipeline",
    });

    ASSERT(pipeline.isValid());
}

void Lighting::load(uint32_t width, uint32_t height)
{
    hz::TextureDesc targetDesc = {
        .width = width,
        .height = height,
        .startState = RESOURCE_STATE_RENDER_TARGET,
        .descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .renderTarget = true,
    };

    targetDesc.format = hz::Format::B10G11R11_UFLOAT;
    targetDesc.pName = "Lighting.SceneColor";
    sceneColor = context.createTexture(targetDesc);
}

void Lighting::unload() { sceneColor = {}; }

void Lighting::update() {}

void Lighting::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const GBuffer& gbuffer, const hz::SceneFrameSnapshot& scene)
{
    const hz::GPUTexture* sampledTextures[] = { &gbuffer.gbuffer[0], &gbuffer.gbuffer[1], &gbuffer.gbuffer[2], &gbuffer.depth };
    commands.beginGpuTimestamp("Lighting");
    commands.beginRendering(
        {
            .colorAttachments = { { .pTexture = &sceneColor,
                                    .loadAction = LOAD_ACTION_CLEAR,
                                    .storeAction = STORE_ACTION_STORE,
                                    .clearValue = { .r = 0.02f, .g = 0.035f, .b = 0.055f, .a = 1.0f } } },
        },
        { .sampledTextures = sampledTextures, .buffers = { &frame } });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    const uint32_t indices[] = { frame.getSrvIndex(),
                                 gbuffer.gbuffer[0].getSrvIndex(),
                                 gbuffer.gbuffer[1].getSrvIndex(),
                                 gbuffer.gbuffer[2].getSrvIndex(),
                                 gbuffer.depth.getSrvIndex(),
                                 scene.getBuffers().lights.isValid() ? scene.getBuffers().lights.getSrvIndex() : UINT32_MAX,
                                 scene.getBuffers().activeLights.isValid() ? scene.getBuffers().activeLights.getSrvIndex() : UINT32_MAX,
                                 scene.getActiveLights().count };
    commands.setRootConstant(indices);
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

PostProcessing::PostProcessing(hz::RenderContext& context): context(context)
{
    pipeline = context.createGraphicsPipeline({
        .shaderDesc={
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.PostProcessVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.PostProcessPS" },
        },
        .pFileName = "PostProcess.slang",
    },
        .colorTargets = { { .format = context.getColorFormat() } },
        .pName = "Renderer.PostProcessPipeline",
    });

    ASSERT(pipeline.isValid());
}

void PostProcessing::load(uint32_t, uint32_t) {}

void PostProcessing::unload() {}

void PostProcessing::update() {}

void PostProcessing::execute(hz::CommandList& commands, const hz::GPUTexture& renderTarget, const hz::GPUTexture& sceneColor,
                             const hz::GPUTexture& depth)
{
    const hz::GPUTexture* sampledTextures[] = { &sceneColor, &depth };
    commands.beginGpuTimestamp("PostProcess");
    commands.beginRendering(
        {
            .colorAttachments = { { .pTexture = &renderTarget, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } },
        },
        { .sampledTextures = sampledTextures });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    const HDRMetadata&             hdrMetadata = context.getHDRMetadata();
    const PostProcessRootConstants constants = {
        .sceneColorIndex = sceneColor.getSrvIndex(),
        .depthIndex = depth.getSrvIndex(),
        .outputMode = (uint32_t)context.getOutputMode(),
        .paperWhiteNits = 203.0f,
        .peakNits = context.isHDREnabled() ? hdrMetadata.maxContentLightLevel : 100.0f,
    };
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

RenderPasses::RenderPasses(const RenderPassesDesc& desc):
    context(*desc.pContext), scene(*desc.pScene), camera(desc.camera), frames(scene, context, { .frameCount = frameBuffers.size() })
{
    ASSERT(context.isValid() && camera.isValid());
    gbuffer = hz::make_unique<GBuffer>(context);
    lighting = hz::make_unique<Lighting>(context);
    for (hz::GPUBuffer& buffer : frameBuffers)
    {
        buffer = context.createBuffer({ .size = sizeof(FrameData),
                                        .elementCount = 1,
                                        .structStride = sizeof(FrameData),
                                        .pName = "Renderer.Frame",
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .descriptors = DESCRIPTOR_TYPE_BUFFER,
                                        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(buffer.isValid());
    }
}

bool RenderPasses::load(uint32_t width, uint32_t height)
{
    if (!width || !height)
        return true;

    gbuffer->load(width, height);
    lighting->load(width, height);
    postprocessing = hz::make_unique<PostProcessing>(context);
    postprocessing->load(width, height);

    scene.resetHistory();
    return postprocessing && postprocessing->pipeline.isValid();
}

void RenderPasses::unload()
{
    gbuffer->unload();
    lighting->unload();
    if (postprocessing)
    {
        postprocessing->unload();
        postprocessing = nullptr;
    }
    scene.resetHistory();
}

void RenderPasses::update()
{
    gbuffer->update();
    lighting->update();
}

static Matrix4 viewProjection(const Matrix4& pose, const hz::Camera& lens, float aspectInverse)
{
    const float horizontalFov = 2.0f * atanf(tanf(lens.verticalFov * 0.5f) / aspectInverse);
    return Matrix4::perspectiveRH(horizontalFov, aspectInverse, lens.nearPlane, lens.farPlane) * inverse(pose);
}
void RenderPasses::execute()
{
    if (context.isSuspended())
        return;

    ASSERT(postprocessing);
    hz::SceneFrameSnapshot snapshot = frames.capture();
    if (!snapshot.isValid())
        return;
    ASSERT(camera.index < snapshot.getCameras().count && snapshot.getCameras().pData[camera.index].id == camera);
    const hz::RenderCamera& cameraData = snapshot.getCameras().pData[camera.index];
    const float             aspectInverse = (float)context.getHeight() / (float)context.getWidth();
    const Matrix4           current = viewProjection(cameraData.current, cameraData.lens, aspectInverse);
    const FrameData         frameData = { .viewProjection = current,
                                          .previousViewProjection = viewProjection(cameraData.previous, cameraData.previousLens, aspectInverse),
                                          .inverseViewProjection = inverse(current),
                                          .eye = cameraData.current.getCol3() };
    hz::GPUBuffer&          frame = frameBuffers[snapshot.getBackingIndex()];
    const bool              updated = context.updateBuffer(frame, 0, &frameData, sizeof(frameData));
    ASSERT(updated);
    hz::CommandList& commands = context.acquireCommandList();
    gbuffer->execute(commands, frame, snapshot);
    const hz::GPUTexture& backbuffer = context.getCurrentBackbuffer();
    lighting->execute(commands, frame, *gbuffer, snapshot);
    postprocessing->execute(commands, backbuffer, lighting->sceneColor, gbuffer->depth);

    frames.retire(std::move(snapshot), context.submit(commands, &backbuffer));
}
