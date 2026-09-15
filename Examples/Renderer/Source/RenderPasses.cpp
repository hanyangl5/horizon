#include "RenderPasses.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Application/IFreeCameraController.h"
#include "Core/ILog.h"

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

void GBuffer::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUBuffer& draws, const hz::GPUBuffer& materials,
                      SceneManager& scenes, SceneAssetHandle scene, hz::Span<const SceneAssetInstance> instances)
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
        .buffers = { &geometry.vertexBuffers[0], &geometry.vertexBuffers[1], &geometry.vertexBuffers[2], &frame, &draws, &materials },
    });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    const uint32_t indices[] = { frame.getSrvIndex(),
                                 draws.getSrvIndex(),
                                 materials.getSrvIndex(),
                                 sampler.getIndex(),
                                 geometry.vertexBuffers[0].getSrvIndex(),
                                 geometry.vertexBuffers[1].getSrvIndex(),
                                 geometry.vertexBuffers[2].getSrvIndex() };
    commands.setPushConstants(0, indices, sizeof(indices));
    commands.setIndexBuffer(geometry.indexBuffer, 0, geometry.indexType);

    for (uint32_t i = 0; i < instances.count; ++i)
    {
        const SceneAssetInstance& instance = instances.pData[i];
        commands.setPushConstants(1, &i, sizeof(i));
        const IndirectDrawIndexArguments& draw = geometry.pDrawArgs[instance.drawIndex];
        commands.drawIndexed(draw.indexCount, draw.startIndex, draw.vertexOffset);
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
        .descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .renderTarget = true,
    };

    targetDesc.format = hz::Format::B10G11R11_UFLOAT;
    targetDesc.pName = "Lighting.SceneColor";
    sceneColor = context.createTexture(targetDesc);
}

void Lighting::unload() { sceneColor = {}; }

void Lighting::update() {}

void Lighting::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const GBuffer& gbuffer)
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
    const uint32_t indices[] = { frame.getSrvIndex(), gbuffer.gbuffer[0].getSrvIndex(), gbuffer.gbuffer[1].getSrvIndex(),
                                 gbuffer.gbuffer[2].getSrvIndex(), gbuffer.depth.getSrvIndex() };
    commands.setPushConstants(0, indices, sizeof(indices));
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
    commands.setPushConstants(0, &constants, sizeof(constants));
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

RenderPasses::RenderPasses(const RenderPassesDesc& desc):
    pContext(*desc.pContext), pScenes(desc.pScenes), scene(desc.scene), instances({ desc.pInstances, desc.instanceCount }),
    verticalFov(desc.verticalFov)
{
    ASSERT(pContext.isValid());
    ASSERT(pScenes);
    ASSERT(isSceneAssetHandleValid(scene));
    ASSERT(desc.pInstances);
    ASSERT(!instances.empty());

    qsort(instances.data(), instances.size(), sizeof(SceneAssetInstance), compareMaterials);
    gbuffer = hz::make_unique<GBuffer>(pContext);
    lighting = hz::make_unique<Lighting>(pContext);
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

    hz::Array<SceneAssetGpuMaterial> gpuMaterials(pScenes->getMaterialCount(scene));
    memcpy(gpuMaterials.data(), pScenes->getGpuMaterials(scene), gpuMaterials.size() * sizeof(SceneAssetGpuMaterial));
    for (SceneAssetGpuMaterial& material : gpuMaterials)
    {
        uint32_t* textureIndices[] = { &material.baseColorTexture, &material.normalTexture, &material.metallicRoughnessTexture,
                                       &material.emissiveTexture };
        for (uint32_t* pIndex : textureIndices)
            if (*pIndex != UINT32_MAX)
                *pIndex = pScenes->getTexture(scene, *pIndex)->getSrvIndex();
    }
    materials = pContext.createBuffer({
        .size = gpuMaterials.size() * sizeof(SceneAssetGpuMaterial),
        .elementCount = gpuMaterials.size(),
        .structStride = sizeof(SceneAssetGpuMaterial),
        .pName = "Renderer.Materials",
        .pInitialData = gpuMaterials.data(),
        .initialDataSize = gpuMaterials.size() * sizeof(SceneAssetGpuMaterial),
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_SHADER_RESOURCE,
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

    ASSERT(draws.isValid() && frame.isValid() && materials.isValid());
    return draws.isValid() && frame.isValid() && materials.isValid();
}

bool RenderPasses::load(uint32_t width, uint32_t height)
{
    if (!width || !height)
        return true;

    gbuffer->load(width, height);
    lighting->load(width, height);
    postprocessing = hz::make_unique<PostProcessing>(pContext);
    postprocessing->load(width, height);

    hasPreviousViewProjection = false;
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

    ASSERT(postprocessing);
    hz::CommandList& commands = pContext.acquireCommandList();
    commands.updateBuffer(frame, 0, &frameData, sizeof(frameData));
    gbuffer->execute(commands, frame, draws, materials, *pScenes, scene, { instances.data(), instances.size() });
    const hz::GPUTexture& backbuffer = pContext.getCurrentBackbuffer();
    lighting->execute(commands, frame, *gbuffer);
    postprocessing->execute(commands, backbuffer, lighting->sceneColor, gbuffer->depth);

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
