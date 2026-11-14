#include "RenderPasses.h"
#include "RendererConstants.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Core/ILog.h"
#include "Profiler/IProfiler.h"
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

static_assert(offsetof(hz::RenderInstance, current) == 32 && offsetof(hz::RenderInstance, previous) == 96);

struct GeometryBindingConstants
{
    uint32_t frameIndex;
    uint32_t instancesIndex;
    uint32_t materialsIndex;
    uint32_t texturesIndex;
    uint32_t samplerIndex;
};
static_assert(sizeof(GeometryBindingConstants) == 5 * sizeof(uint32_t));

GBuffer::GBuffer(hz::RenderContext& context): context(context)
{
    pipeline = context.createGraphicsPipeline({
    .shaderDesc = {
        .stages = {
            { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect", .pName = "Renderer.GeometryVS" },
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
    .indirectDrawId = true,
    .pName = "Renderer.GeometryPipeline",
});

    sampler = context.createSampler();
    ASSERT(pipeline.isValid() && sampler.isValid());
}

void GBuffer::resize(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    hz::TextureDesc targetDesc = {
        .width = width,
        .height = height,
        .startState = RESOURCE_STATE_RENDER_TARGET,
        .descriptors = DESCRIPTOR_TYPE_TEXTURE,
        .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
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

void GBuffer::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const SceneView& view, const GpuDrawList& draws)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GBuffer::execute");
    const hz::GpuSceneBuffers& gpu = view.gpuScene.getBuffers();
    const ClearValue           black = {};
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
    }, draws.getDependencies(frame, view, &draws.getCommands(view.frameIndex)));
    commands.setViewport(0, 0, (float)width, (float)height);
    commands.setScissor(0, 0, width, height);
    commands.setPipeline(pipeline);
    const GeometryBindingConstants binding = {
        .frameIndex = frame.getSrvIndex(),
        .instancesIndex = gpu.instances.isValid() ? gpu.instances.getSrvIndex() : UINT32_MAX,
        .materialsIndex = gpu.materials.isValid() ? gpu.materials.getSrvIndex() : UINT32_MAX,
        .texturesIndex = gpu.textures.isValid() ? gpu.textures.getSrvIndex() : UINT32_MAX,
        .samplerIndex = sampler.getIndex(),
    };
    commands.setRootConstants(binding, kGeometryBindingSlot);

    commands.setRootConstant({ draws.getDrawData(view.frameIndex).getSrvIndex() }, kGeometryDrawSlot);
    if (draws.getCount(view.frameIndex))
    {
        commands.drawIndirect(draws.getCommands(view.frameIndex), 0, draws.getCount(view.frameIndex));
    }
    commands.endRendering();
    commands.endGpuTimestamp();
}

static void executeShadows(FrameContext& frame, void* pUserData)
{
    ShadowMap& shadows = *(ShadowMap*)pUserData;
    shadows.execute(frame.commands, frame.view, frame.draws);
    frame.environmentIntensity = shadows.data.environmentIntensity;
}

static void executeGBuffer(FrameContext& frame, void* pUserData)
{
    GBuffer& gbuffer = *(GBuffer*)pUserData;
    gbuffer.execute(frame.commands, frame.frame, frame.view, frame.draws);
    frame.gbuffer = { .pEmissive = &gbuffer.gbuffer[0],
                      .pNormalMaterial = &gbuffer.gbuffer[1],
                      .pBaseColorMetallic = &gbuffer.gbuffer[2],
                      .pDepth = &gbuffer.depth };
}

static void executeScreenSpaceAO(FrameContext& frame, void* pUserData)
{
    // Ray traced AO replaces the screen space term.
    if (!frame.rayTracedAO)
    {
        frame.resources.pSSAO =
            &((ScreenSpaceAO*)pUserData)->execute(frame.commands, frame.frame, *frame.gbuffer.pDepth, frame.linearSampler);
    }
}

static void executeRayTracing(FrameContext& frame, void* pUserData)
{
    RayTracedEffects& rayTracing = *(RayTracedEffects*)pUserData;
    rayTracing.execute(frame.commands, frame.frame, frame.gbuffer, frame.view, frame.draws, frame.resources);
    if (!rayTracing.isSupported())
    {
        return;
    }
    if (rayTracing.settings.shadows || rayTracing.settings.ao)
    {
        frame.resources.pRTShadowAO = &rayTracing.getShadowAO();
    }
    if (rayTracing.settings.reflection)
    {
        frame.resources.pRTReflection = &rayTracing.getReflection();
    }
}

static void executeLighting(FrameContext& frame, void* pUserData)
{
    frame.pColor = &((DeferredLighting*)pUserData)->execute(frame.commands, frame.frame, frame.gbuffer, frame.view, frame.resources);
}

static void executeEnvironment(FrameContext& frame, void* pUserData)
{
    ((EnvironmentPass*)pUserData)
        ->execute(frame.commands, frame.frame, *frame.pColor, *frame.gbuffer.pDepth, frame.environmentIntensity);
}

static void executeTransparency(FrameContext& frame, void* pUserData)
{
    frame.pColor = &((Transparency*)pUserData)
                        ->execute(frame.commands, frame.frame, *frame.gbuffer.pDepth, frame.view, frame.draws, frame.resources,
                                  *frame.pColor);
}

static void executePost(FrameContext& frame, void* pUserData)
{
    ((PostEffects*)pUserData)->execute(frame.commands, *frame.pColor, *frame.pBackbuffer, frame.deltaSeconds);
}

RenderPasses::RenderPasses(const RenderPassesDesc& desc):
    context(*desc.pContext), scene(*desc.pScene), assets(*desc.pAssetManager), camera(desc.camera), draws(context),
    gpuScene(scene, assets, context, { .frameCount = frameConstants.size() })
{
    ASSERT(context.isValid() && camera.isValid());
    gbuffer = hz::make_unique<GBuffer>(context);
    lighting = hz::make_unique<DeferredLighting>(context);
    ssao = hz::make_unique<ScreenSpaceAO>(context);
    environment = hz::make_unique<EnvironmentPass>(context, hz::Format::R32G32B32A32_SFLOAT);
    shadows = hz::make_unique<ShadowMap>(context);
    postprocessing = hz::make_unique<PostEffects>(context);
    transparency = hz::make_unique<Transparency>(context);
    rayTracing = hz::make_unique<RayTracedEffects>(context);
    addPass({ .pName = "Shadows", .pExecute = executeShadows, .pUserData = shadows.get() });
    addPass({ .pName = "GBuffer", .pExecute = executeGBuffer, .pUserData = gbuffer.get() });
    addPass({ .pName = "SSAO", .pExecute = executeScreenSpaceAO, .pUserData = ssao.get(), .pEnabled = &enableScreenSpaceAO });
    addPass({ .pName = "Ray Tracing", .pExecute = executeRayTracing, .pUserData = rayTracing.get() });
    addPass({ .pName = "Lighting", .pExecute = executeLighting, .pUserData = lighting.get() });
    addPass({ .pName = "Environment", .pExecute = executeEnvironment, .pUserData = environment.get() });
    addPass({ .pName = "Transparency", .pExecute = executeTransparency, .pUserData = transparency.get(), .pEnabled = &enableTransparency });
    addPass({ .pName = "Post", .pExecute = executePost, .pUserData = postprocessing.get() });

    linearSampler = context.createSampler({ .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE });
    for (FrameConstants& constants : frameConstants)
    {
        hz::GPUBuffer& buffer = constants.buffer;
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

bool RenderPasses::resize(uint32_t width, uint32_t height)
{
    if (!width || !height)
    {
        return true;
    }

    if (!environment->isValid() && !environment->load({ .directory = RD_SHADER_SOURCES,
                                                        .prefiltered = "../../../Assets/Cookbook/immenstadter_horn_2k_prefilter.ktx",
                                                        .irradiance = "../../../Assets/Cookbook/immenstadter_horn_2k_irradiance.ktx",
                                                        .brdfLut = "../../../Assets/Cookbook/brdfLUT.ktx" }))
    {
        return false;
    }
    if (!ssao->resize(width, height))
    {
        return false;
    }
    gbuffer->resize(width, height);
    lighting->resize(width, height);
    transparency->resize(width, height, 1);
    rayTracing->resize(width, height);
    postprocessing->resize(width, height);

    scene.markDirty();
    return true;
}

void RenderPasses::update(float frameDeltaSeconds) { deltaSeconds = frameDeltaSeconds; }

static Matrix4 viewProjection(const Matrix4& pose, const hz::Camera& lens, float aspectInverse)
{
    const float horizontalFov = 2.0f * atanf(tanf(lens.verticalFov * 0.5f) / aspectInverse);
    return Matrix4::perspectiveRH(horizontalFov, aspectInverse, lens.nearPlane, lens.farPlane) * inverse(pose);
}
void RenderPasses::execute()
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::execute");
    if (context.isSuspended())
    {
        return;
    }

    ASSERT(postprocessing);
    // Block on the oldest frame like Unreal's frame sync; returning would spin Update/Draw while every frame is in flight.
    const uint32_t  frameIndex = nextFrame;
    FrameConstants& constants = frameConstants[frameIndex];
    if (constants.submission.isValid())
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RenderPasses::waitForFrame");
        context.wait(constants.submission);
    }
    if (!gpuScene.beginFrame())
    {
        return;
    }
    nextFrame = (nextFrame + 1) % frameConstants.size();
    const SceneView         view = { .scene = scene, .gpuScene = gpuScene, .assets = assets, .frameIndex = frameIndex };
    const hz::RenderCamera* pCamera = scene.getCamera(camera);
    ASSERT(pCamera);
    const hz::RenderCamera& cameraData = *pCamera;
    const float             aspectInverse = (float)context.getHeight() / (float)context.getWidth();
    const Matrix4           current = viewProjection(cameraData.current, cameraData.lens, aspectInverse);
    const FrameData         frameData = { .viewProjection = current,
                                          .previousViewProjection = viewProjection(cameraData.previous, cameraData.previousLens, aspectInverse),
                                          .inverseViewProjection = inverse(current),
                                          .eye = cameraData.current.getCol3() };
    const hz::GPUBuffer&    frame = constants.buffer;
    const bool              updated = context.updateBuffer(frame, 0, &frameData, sizeof(frameData));
    ASSERT(updated);
    hz::CommandList& commands = context.acquireCommandList();
    gpuScene.upload(commands);
    draws.prepare(commands, frame, view);
    // Frame shape the passes depend on; the shadow pass publishes its intensity once it ran.
    shadows->data.environmentMipCount = environment->getPrefilteredMipCount();
    shadows->data.rtShadowEnabled = rayTracing->isSupported() && rayTracing->settings.shadows ? 1u : 0u;
    shadows->data.rtAOEnabled = rayTracing->isSupported() && rayTracing->settings.ao ? 1u : 0u;
    const hz::GPUTexture& backbuffer = context.getCurrentBackbuffer();
    FrameContext          frameContext = { .commands = commands,
                                           .draws = draws,
                                           .frame = frame,
                                           .view = view,
                                           .linearSampler = linearSampler,
                                           .deltaSeconds = deltaSeconds,
                                           .rayTracedAO = shadows->data.rtAOEnabled != 0,
                                           .pBackbuffer = &backbuffer,
                                           .resources = { .pLightingFrame = &shadows->getLightingFrame(frameIndex),
                                                          .pShadowMap = &shadows->getTexture(),
                                                          .pShadowSampler = &shadows->getSampler(),
                                                          .pEnvironment = &environment->getPrefiltered(),
                                                          .pIrradiance = &environment->getIrradiance(),
                                                          .pBRDFLut = &environment->getBrdfLut(),
                                                          .pEnvironmentSampler = &environment->getSampler() } };
    for (uint32_t i = 0; i < passCount; ++i)
    {
        const RenderPass& pass = passes[i];
        if (pass.pEnabled && !*pass.pEnabled)
            continue;
        pass.pExecute(frameContext, pass.pUserData);
    }
    ASSERT(frameContext.pColor && frameContext.pBackbuffer);
    constants.submission = context.submit(commands, frameContext.pBackbuffer);
    gpuScene.endFrame(constants.submission);
}