#include "RenderPasses.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Application/IFreeCameraController.h"
#include "Core/ILog.h"
#include "Core/IMemory.h"

constexpr TinyImageFormat kDepthFormat = TinyImageFormat_D24_UNORM_S8_UINT;
constexpr TinyImageFormat kGBufferFormats[] = {
    TinyImageFormat_R10G10B10A2_UNORM,
    TinyImageFormat_R10G10B10A2_UNORM,
    TinyImageFormat_R8G8B8A8_UNORM,
    TinyImageFormat_R8G8B8A8_UNORM,
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
    const uint32_t a = ((const SceneAssetInstance*)left)->mMaterialIndex;
    const uint32_t b = ((const SceneAssetInstance*)right)->mMaterialIndex;
    return (a > b) - (a < b);
}

GBuffer::GBuffer(hz::RenderContext& pContext, const VertexLayout& vertexLayout): pContext(pContext)
{
    mGeometryShader = pContext.createShader({
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.GeometryVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.GeometryPS" },
        },
        .stageCount = 2,
        .pFileName = "Geometry.hlsl",
    });
    mGeometryPipeline = pContext.createGraphicsPipeline({
        .pShader = &mGeometryShader,
        .vertexLayout = vertexLayout,
        .depth = { .mDepthTest = true, .mDepthWrite = true, .mDepthFunc = CMP_LEQUAL },
        .colorFormats = { kGBufferFormats[0], kGBufferFormats[1], kGBufferFormats[2], kGBufferFormats[3] },
        .renderTargetCount = GBufferCount,
        .depthStencilFormat = kDepthFormat,
        .pName = "Renderer.GeometryPipeline",
    });
    mSampler = pContext.createSampler();
    ASSERT(mGeometryShader.isValid() && mGeometryPipeline.isValid() && mSampler.isValid());
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
    for (uint32_t i = 0; i < GBuffer::GBufferCount; ++i)
    {
        targetDesc.format = kGBufferFormats[i];
        targetDesc.pName = kGBufferNames[i];
        mGBuffer[i] = pContext.createTexture(targetDesc);
    }

    targetDesc.format = kDepthFormat;
    targetDesc.startState = RESOURCE_STATE_DEPTH_WRITE;
    targetDesc.pName = "Renderer.Depth";
    mDepth = pContext.createTexture(targetDesc);
}

void GBuffer::unload()
{
    for (hz::GPUTexture& target : mGBuffer)
        target = {};
    mDepth = {};
}

void GBuffer::update() {}

void GBuffer::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUBuffer& draws,
                      SceneManager& scenes, SceneAssetHandle scene, hz::Span<const SceneAssetInstance> instances)
{
    const SceneGeometry* pGeometry = scenes.getGeometry(scene);
    ASSERT(pGeometry);
    const SceneGeometry& geometry = *pGeometry;
    const ClearValue black = {};
    const hz::RenderPassDesc geometryPass = {
        .colorAttachments = {
            { .pTexture = &mGBuffer[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            { .pTexture = &mGBuffer[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            { .pTexture = &mGBuffer[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
            { .pTexture = &mGBuffer[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE, .clearValue = black },
        },
        .colorAttachmentCount = GBufferCount,
        .depthAttachment = { .pTexture = &mDepth,
                             .loadAction = LOAD_ACTION_CLEAR,
                             .storeAction = STORE_ACTION_STORE,
                             .clearValue = { .depth = 1.0f } },
    };
    commands.beginGpuTimestamp("Geometry");
    commands.beginRendering(geometryPass);
    commands.setViewport(0, 0, (float)pContext.getWidth(), (float)pContext.getHeight());
    commands.setScissor(0, 0, pContext.getWidth(), pContext.getHeight());
    commands.setPipeline(mGeometryPipeline);
    commands.bindBuffer("Frame", frame);
    commands.bindBuffer("Draws", draws);
    commands.bindBuffer("Materials", *scenes.getMaterialBuffer(scene));
    commands.bindSampler("SurfaceSampler", mSampler);
    for (uint32_t i = 0; i < geometry.mVertexBufferCount; ++i)
        commands.setVertexBuffer(i, geometry.mVertexBuffers[i], 0, geometry.mVertexStrides[i]);
    commands.setIndexBuffer(geometry.mIndexBuffer, 0, geometry.mIndexType);

    const SceneAssetGpuMaterial* gpuMaterials = scenes.getGpuMaterials(scene);
    uint32_t                     previousMaterial = UINT32_MAX;
    for (uint32_t i = 0; i < instances.count; ++i)
    {
        const SceneAssetInstance& instance = instances.pData[i];
        if (instance.mMaterialIndex != previousMaterial)
        {
            const SceneAssetGpuMaterial& material = gpuMaterials[instance.mMaterialIndex];
            const uint32_t textureIndices[] = { material.mBaseColorTexture, material.mNormalTexture, material.mMetallicRoughnessTexture,
                                                material.mEmissiveTexture };
            const char*    names[] = { "BaseColor", "NormalMap", "MetallicRoughness", "Emissive" };
            for (uint32_t t = 0; t < TF_ARRAY_COUNT(textureIndices); ++t)
            {
                const uint32_t textureIndex = textureIndices[t] == UINT32_MAX ? 0 : textureIndices[t];
                commands.bindTexture(names[t], *scenes.getTexture(scene, textureIndex));
            }
            previousMaterial = instance.mMaterialIndex;
        }
        commands.setPushConstants(0, &i, sizeof(i));
        const IndirectDrawIndexArguments& draw = geometry.pDrawArgs[instance.mDrawIndex];
        commands.drawIndexed(draw.mIndexCount, draw.mStartIndex, draw.mVertexOffset);
    }
    commands.endRendering();
    commands.endGpuTimestamp();
}

Lighting::Lighting(hz::RenderContext& pContext, TinyImageFormat format): pContext(pContext)
{
    mLightingShader = pContext.createShader({
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain", .pName = "Renderer.LightingVS" },
            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain", .pName = "Renderer.LightingPS" },
        },
        .stageCount = 2,
        .pFileName = "Lighting.hlsl",
    });
    mLightingPipeline = pContext.createGraphicsPipeline({
        .pShader = &mLightingShader,
        .colorFormats = { format },
        .renderTargetCount = 1,
        .pName = "Renderer.LightingPipeline",
    });
    ASSERT(mLightingShader.isValid() && mLightingPipeline.isValid());
}

void Lighting::load(uint32_t width, uint32_t height) {}

void Lighting::unload() {}

void Lighting::update() {}

void Lighting::execute(hz::CommandList& commands, const hz::GPUTexture& renderTarget, const hz::GPUBuffer& frame, const GBuffer& gbuffer)
{
    const hz::RenderPassDesc lightingPass = {
        .colorAttachments = { { .pTexture = &renderTarget,
                                .loadAction = LOAD_ACTION_CLEAR,
                                .storeAction = STORE_ACTION_STORE,
                                .clearValue = { .r = 0.02f, .g = 0.035f, .b = 0.055f, .a = 1.0f } } },
        .colorAttachmentCount = 1,
    };
    const hz::GPUTexture* sampledTextures[] = { &gbuffer.mGBuffer[0], &gbuffer.mGBuffer[1], &gbuffer.mGBuffer[2], &gbuffer.mDepth };
    commands.beginGpuTimestamp("Lighting");
    commands.beginRendering(lightingPass, { .sampledTextures = sampledTextures });
    commands.setViewport(0, 0, (float)pContext.getWidth(), (float)pContext.getHeight());
    commands.setScissor(0, 0, pContext.getWidth(), pContext.getHeight());
    commands.setPipeline(mLightingPipeline);
    commands.bindTexture("GBuffer0", gbuffer.mGBuffer[0]);
    commands.bindTexture("GBuffer1", gbuffer.mGBuffer[1]);
    commands.bindTexture("GBuffer2", gbuffer.mGBuffer[2]);
    commands.bindTexture("SceneDepth", gbuffer.mDepth);
    commands.bindBuffer("Frame", frame);
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
}

RenderPasses::RenderPasses(const RenderPassesDesc& desc):
    pContext(*desc.pContext), pScenes(desc.pScenes), mScene(desc.scene), mInstanceCount(desc.instanceCount),
    mSurfaceFormat(desc.surfaceFormat), mVerticalFov(desc.verticalFov)
{
    ASSERT(pContext.isValid());
    ASSERT(pScenes);
    ASSERT(isSceneAssetHandleValid(mScene));
    ASSERT(desc.pInstances);
    ASSERT(mInstanceCount);
    ASSERT(desc.pVertexLayout);
    ASSERT(mSurfaceFormat != TinyImageFormat_UNDEFINED);

    pInstances = (SceneAssetInstance*)tf_malloc(mInstanceCount * sizeof(SceneAssetInstance));
    memcpy(pInstances, desc.pInstances, mInstanceCount * sizeof(SceneAssetInstance));
    qsort(pInstances, mInstanceCount, sizeof(SceneAssetInstance), compareMaterials);
    gbuffer = hz::make_unique<GBuffer>(pContext, *desc.pVertexLayout);
    lighting = hz::make_unique<Lighting>(pContext, mSurfaceFormat);
    const bool inited = initRenderResources();
    ASSERT(inited && gbuffer && lighting);
}

RenderPasses::~RenderPasses() { tf_free(pInstances); }

bool RenderPasses::initRenderResources()
{
    const SceneGeometry& geometry = getGeometry();
    DrawData*            draws = (DrawData*)tf_calloc(mInstanceCount, sizeof(DrawData));
    for (uint32_t i = 0; i < mInstanceCount; ++i)
    {
        if (pInstances[i].mDrawIndex >= geometry.mDrawArgCount || pInstances[i].mMaterialIndex >= pScenes->getMaterialCount(mScene))
        {
            tf_free(draws);
            LOGF(eERROR, "Scene instance has an invalid draw or material index");
            return false;
        }
        const float* world = pInstances[i].mWorld;
        draws[i].world = Matrix4(world[0], world[1], world[2], world[3], world[4], world[5], world[6], world[7], world[8], world[9],
                                 world[10], world[11], world[12], world[13], world[14], world[15]);
        draws[i].normal = transpose(inverse(draws[i].world));
        draws[i].material = pInstances[i].mMaterialIndex;
        draws[i].alphaCutoff = pInstances[i].mAlphaCutoff;
    }

    mDraws = pContext.createBuffer({
        .size = mInstanceCount * sizeof(DrawData),
        .elementCount = mInstanceCount,
        .structStride = sizeof(DrawData),
        .pName = "Renderer.Instances",
        .pInitialData = draws,
        .initialDataSize = mInstanceCount * sizeof(DrawData),
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_SHADER_RESOURCE,
        .descriptors = DESCRIPTOR_TYPE_BUFFER,
    });
    tf_free(draws);

    mFrame = pContext.createBuffer({
        .size = sizeof(FrameData),
        .elementCount = 1,
        .structStride = sizeof(FrameData),
        .pName = "Renderer.Frame",
        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .startState = RESOURCE_STATE_SHADER_RESOURCE,
        .descriptors = DESCRIPTOR_TYPE_BUFFER,
    });

    ASSERT(mDraws.isValid() && mFrame.isValid());
    return mDraws.isValid() && mFrame.isValid();
}

bool RenderPasses::load(uint32_t width, uint32_t height)
{
    if (!width || !height)
        return true;

    gbuffer->load(width, height);
    lighting->load(width, height);

    mHasPreviousViewProjection = false;
    return true;
}

void RenderPasses::unload()
{
    gbuffer->unload();
    lighting->unload();
    mHasPreviousViewProjection = false;
}

void RenderPasses::update(const FreeCameraController& camera)
{
    if (pContext.isSuspended())
        return;

    const float   aspectInverse = (float)pContext.getHeight() / (float)pContext.getWidth();
    const float   horizontalFov = 2.0f * atanf(tanf(mVerticalFov * 0.5f) / aspectInverse);
    const Matrix4 viewProjection = Matrix4::perspectiveRH(horizontalFov, aspectInverse, 0.1f, 1000.0f) *
                                   Matrix4::scale(Vector3(-1.0f, 1.0f, -1.0f)) * camera.getViewMatrix();
    mFrameData = {
        .viewProjection = viewProjection,
        .previousViewProjection = mHasPreviousViewProjection ? mPreviousViewProjection : viewProjection,
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
    commands.updateBuffer(mFrame, 0, &mFrameData, sizeof(mFrameData));
    gbuffer->execute(commands, mFrame, mDraws, *pScenes, mScene, { pInstances, mInstanceCount });
    const hz::GPUTexture& backbuffer = pContext.getCurrentBackbuffer();
    lighting->execute(commands, backbuffer, mFrame, *gbuffer);

    pContext.submit(commands, &backbuffer);
    mPreviousViewProjection = mFrameData.viewProjection;
    mHasPreviousViewProjection = true;
}

const SceneGeometry& RenderPasses::getGeometry() const
{
    const SceneGeometry* geometry = pScenes->getGeometry(mScene);
    ASSERT(geometry);
    return *geometry;
}
