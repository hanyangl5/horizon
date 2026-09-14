#include "RenderPasses.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Application/IFreeCameraController.h"
#include "Core/ILog.h"

constexpr hz::Format kDepthFormat = hz::Format::D24_UNORM_S8_UINT;
constexpr hz::Format kGBufferFormats[] = {
    hz::Format::R10G10B10A2_UNORM,
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

struct DrawData
{
    Matrix4  world;
    Matrix4  normal;
    uint32_t material;
    float    alphaCutoff;
    uint32_t padding[2];
};

static int compareMaterials(const void* left, const void* right)
{
    const uint32_t a = ((const SceneAssetInstance*)left)->materialIndex;
    const uint32_t b = ((const SceneAssetInstance*)right)->materialIndex;
    return (a > b) - (a < b);
}

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

void GBuffer::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUBuffer& draws, SceneManager& scenes,
                      SceneAssetHandle scene, hz::Span<const SceneAssetInstance> instances)
{
    const SceneGeometry* pGeometry = scenes.getGeometry(scene);
    ASSERT(pGeometry);
    const SceneGeometry& geometry = *pGeometry;
    const ClearValue     black = {};
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
        .buffers = { &geometry.vertexBuffers[0], &geometry.vertexBuffers[1], &geometry.vertexBuffers[2] },
    });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    commands.bindBuffer("Frame", frame);
    commands.bindBuffer("Draws", draws);
    commands.bindBuffer("Materials", *scenes.getMaterialBuffer(scene));
    commands.bindSampler("SurfaceSampler", sampler);
    commands.bindBuffer("Positions", geometry.vertexBuffers[0]);
    commands.bindBuffer("Normals", geometry.vertexBuffers[1]);
    commands.bindBuffer("Texcoords", geometry.vertexBuffers[2]);
    commands.setIndexBuffer(geometry.indexBuffer, 0, geometry.indexType);

    const SceneAssetGpuMaterial* gpuMaterials = scenes.getGpuMaterials(scene);
    uint32_t                     previousMaterial = UINT32_MAX;
    for (uint32_t i = 0; i < instances.count; ++i)
    {
        const SceneAssetInstance& instance = instances.pData[i];
        if (instance.materialIndex != previousMaterial)
        {
            const SceneAssetGpuMaterial& material = gpuMaterials[instance.materialIndex];
            const uint32_t textureIndices[] = { material.baseColorTexture, material.normalTexture, material.metallicRoughnessTexture,
                                                material.emissiveTexture };
            const char*    names[] = { "BaseColor", "NormalMap", "MetallicRoughness", "Emissive" };
            for (uint32_t t = 0; t < TF_ARRAY_COUNT(textureIndices); ++t)
            {
                const uint32_t textureIndex = textureIndices[t] == UINT32_MAX ? 0 : textureIndices[t];
                commands.bindTexture(names[t], *scenes.getTexture(scene, textureIndex));
            }
            previousMaterial = instance.materialIndex;
        }
        commands.setPushConstants(0, &i, sizeof(i));
        const IndirectDrawIndexArguments& draw = geometry.pDrawArgs[instance.drawIndex];
        commands.drawIndexed(draw.indexCount, draw.startIndex, draw.vertexOffset);
    }
    commands.endRendering();
    commands.endGpuTimestamp();
}

Lighting::Lighting(hz::RenderContext& context, hz::Format format): context(context)
{
    pipeline = context.createGraphicsPipeline({
        .shaderDesc={
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.LightingVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.LightingPS" },
        },
        .pFileName = "Lighting.slang",
    },
        .colorTargets = { { .format = format } },
        .pName = "Renderer.LightingPipeline",
    });
    ASSERT(pipeline.isValid());
}

void Lighting::load(uint32_t width, uint32_t height) {}

void Lighting::unload() {}

void Lighting::update() {}

void Lighting::execute(hz::CommandList& commands, const hz::GPUTexture& renderTarget, const hz::GPUBuffer& frame, const GBuffer& gbuffer)
{
    const hz::GPUTexture* sampledTextures[] = { &gbuffer.gbuffer[0], &gbuffer.gbuffer[1], &gbuffer.gbuffer[2], &gbuffer.depth };
    commands.beginGpuTimestamp("Lighting");
    commands.beginRendering(
        {
            .colorAttachments = { { .pTexture = &renderTarget,
                                    .loadAction = LOAD_ACTION_CLEAR,
                                    .storeAction = STORE_ACTION_STORE,
                                    .clearValue = { .r = 0.02f, .g = 0.035f, .b = 0.055f, .a = 1.0f } } },
        },
        { .sampledTextures = sampledTextures });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    commands.bindTexture("GBuffer0", gbuffer.gbuffer[0]);
    commands.bindTexture("GBuffer1", gbuffer.gbuffer[1]);
    commands.bindTexture("GBuffer2", gbuffer.gbuffer[2]);
    commands.bindTexture("SceneDepth", gbuffer.depth);
    commands.bindBuffer("Frame", frame);
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

RenderPasses::RenderPasses(const RenderPassesDesc& desc):
    pContext(*desc.pContext), pScenes(desc.pScenes), scene(desc.scene), instances({ desc.pInstances, desc.instanceCount }),
    surfaceFormat(desc.surfaceFormat), verticalFov(desc.verticalFov)
{
    ASSERT(pContext.isValid());
    ASSERT(pScenes);
    ASSERT(isSceneAssetHandleValid(scene));
    ASSERT(desc.pInstances);
    ASSERT(!instances.empty());
    ASSERT(surfaceFormat != hz::Format::UNDEFINED);

    qsort(instances.data(), instances.size(), sizeof(SceneAssetInstance), compareMaterials);
    gbuffer = hz::make_unique<GBuffer>(pContext);
    lighting = hz::make_unique<Lighting>(pContext, surfaceFormat);
    const bool inited = initRenderResources();
    ASSERT(inited && gbuffer && lighting);
}

bool RenderPasses::initRenderResources()
{
    const SceneGeometry& geometry = getGeometry();
    hz::Array<DrawData>  drawData(instances.size());
    for (uint32_t i = 0; i < instances.size(); ++i)
    {
        if (instances[i].drawIndex >= geometry.drawArgCount || instances[i].materialIndex >= pScenes->getMaterialCount(scene))
        {
            LOGF(eERROR, "Scene instance has an invalid draw or material index");
            return false;
        }
        const float* world = instances[i].world;
        drawData[i].world = Matrix4(world[0], world[1], world[2], world[3], world[4], world[5], world[6], world[7], world[8], world[9],
                                    world[10], world[11], world[12], world[13], world[14], world[15]);
        drawData[i].normal = transpose(inverse(drawData[i].world));
        drawData[i].material = instances[i].materialIndex;
        drawData[i].alphaCutoff = instances[i].alphaCutoff;
    }

    draws = pContext.createBuffer({
        .size = drawData.size() * sizeof(DrawData),
        .elementCount = drawData.size(),
        .structStride = sizeof(DrawData),
        .pName = "Renderer.Instances",
        .pInitialData = drawData.data(),
        .initialDataSize = drawData.size() * sizeof(DrawData),
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .descriptors = DESCRIPTOR_TYPE_BUFFER,
    });

    frame = pContext.createBuffer({
        .size = sizeof(FrameData),
        .elementCount = 1,
        .structStride = sizeof(FrameData),
        .pName = "Renderer.Frame",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .descriptors = DESCRIPTOR_TYPE_BUFFER,
    });

    ASSERT(draws.isValid() && frame.isValid());
    return draws.isValid() && frame.isValid();
}

bool RenderPasses::load(uint32_t width, uint32_t height)
{
    if (!width || !height)
        return true;

    gbuffer->load(width, height);
    lighting->load(width, height);

    hasPreviousViewProjection = false;
    return true;
}

void RenderPasses::unload()
{
    gbuffer->unload();
    lighting->unload();
    hasPreviousViewProjection = false;
}

void RenderPasses::update(const FreeCameraController& camera)
{
    if (pContext.isSuspended())
        return;

    const float   aspectInverse = (float)pContext.getHeight() / (float)pContext.getWidth();
    const float   horizontalFov = 2.0f * atanf(tanf(verticalFov * 0.5f) / aspectInverse);
    const Matrix4 viewProjection = Matrix4::perspectiveRH(horizontalFov, aspectInverse, 0.1f, 1000.0f) *
                                   Matrix4::scale(Vector3(-1.0f, 1.0f, -1.0f)) * camera.getViewMatrix();
    frameData = {
        .viewProjection = viewProjection,
        .previousViewProjection = hasPreviousViewProjection ? previousViewProjection : viewProjection,
        .inverseViewProjection = inverse(viewProjection),
        .eye = Vector4(camera.getPosition(), 1.0f),
    };
    gbuffer->update();
    lighting->update();
}

void RenderPasses::execute()
{
    if (pContext.isSuspended())
        return;

    hz::CommandList& commands = pContext.acquireCommandList();
    commands.updateBuffer(frame, 0, &frameData, sizeof(frameData));
    gbuffer->execute(commands, frame, draws, *pScenes, scene, { instances.data(), instances.size() });
    const hz::GPUTexture& backbuffer = pContext.getCurrentBackbuffer();
    lighting->execute(commands, backbuffer, frame, *gbuffer);

    pContext.submit(commands, &backbuffer);
    previousViewProjection = frameData.viewProjection;
    hasPreviousViewProjection = true;
}

const SceneGeometry& RenderPasses::getGeometry() const
{
    const SceneGeometry* geometry = pScenes->getGeometry(scene);
    ASSERT(geometry);
    return *geometry;
}
