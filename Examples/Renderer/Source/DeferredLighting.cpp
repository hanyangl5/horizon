// Adapted from Cookbook lighting.frag.slang; see ../Cookbook-LICENSE.txt.
#include "DeferredLighting.h"
#include "Profiler/IProfiler.h"

static uint32_t lightingSamplerIndex(const hz::GPUSampler* sampler)
{
    return sampler && sampler->isValid() ? sampler->getIndex() : UINT32_MAX;
}

// Mirrors the RootConstant0 block of DeferredLighting.slang field for field.
struct DeferredLightingConstants
{
    uint32_t frameIndex;
    uint32_t lightingFrameIndex;
    uint32_t emissiveIndex;
    uint32_t normalMaterialIndex;
    uint32_t baseColorMetallicIndex;
    uint32_t depthIndex;
    uint32_t shadowMapIndex;
    uint32_t shadowSamplerIndex;
    uint32_t environmentIndex;
    uint32_t irradianceIndex;
    uint32_t brdfLutIndex;
    uint32_t environmentSamplerIndex;
    uint32_t rtShadowAOIndex;
    uint32_t rtReflectionIndex;
    uint32_t ssaoIndex;
};
static_assert(sizeof(DeferredLightingConstants) == 15 * sizeof(uint32_t));

DeferredLighting::DeferredLighting(hz::RenderContext& context): context(context)
{
    pipeline = context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                                .pFileName = "DeferredLighting.slang" },
                                                .colorTargets = { { .format = hz::Format::R32G32B32A32_SFLOAT } },
                                                .pName = "Renderer.DeferredLighting" });
    ASSERT(pipeline.isValid());
}
void DeferredLighting::resize(uint32_t width, uint32_t height)
{
    color = context.createTexture({ .width = width,
                                    .height = height,
                                    .format = hz::Format::R32G32B32A32_SFLOAT,
                                    .startState = RESOURCE_STATE_RENDER_TARGET,
                                    .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                    .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                    .renderTarget = true,
                                    .pName = "Renderer.DeferredLighting.Color" });
    ASSERT(color.isValid());
}
const hz::GPUTexture& DeferredLighting::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const DeferredGBuffer& gbuffer,
                                                const SceneView& view, const LightingResources& resources)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("DeferredLighting::execute");
    ASSERT(color.isValid() && resources.pLightingFrame && resources.pLightingFrame->isValid());
    ASSERT(gbuffer.pEmissive && gbuffer.pNormalMaterial && gbuffer.pBaseColorMetallic && gbuffer.pDepth);
    ASSERT(resources.pEnvironment && resources.pIrradiance && resources.pBRDFLut && resources.pEnvironmentSampler);
    hz::DependencyList dependencies;
    const DeferredLightingConstants constants = {
        .frameIndex = dependencies.read(frame),
        .lightingFrameIndex = dependencies.read(*resources.pLightingFrame),
        .emissiveIndex = dependencies.sample(gbuffer.pEmissive),
        .normalMaterialIndex = dependencies.sample(gbuffer.pNormalMaterial),
        .baseColorMetallicIndex = dependencies.sample(gbuffer.pBaseColorMetallic),
        .depthIndex = dependencies.sample(gbuffer.pDepth),
        .shadowMapIndex = dependencies.sample(resources.pShadowMap),
        .shadowSamplerIndex = lightingSamplerIndex(resources.pShadowSampler),
        .environmentIndex = dependencies.sample(resources.pEnvironment),
        .irradianceIndex = dependencies.sample(resources.pIrradiance),
        .brdfLutIndex = dependencies.sample(resources.pBRDFLut),
        .environmentSamplerIndex = lightingSamplerIndex(resources.pEnvironmentSampler),
        .rtShadowAOIndex = dependencies.sample(resources.pRTShadowAO),
        .rtReflectionIndex = dependencies.sample(resources.pRTReflection),
        .ssaoIndex = dependencies.sample(resources.pSSAO),
    };
    commands.beginGpuTimestamp("DeferredLighting");
    // Keep graphics state active during GBV attachment-transition checks after DXR.
    commands.setPipeline(pipeline);
    commands.beginRendering(
        { .colorAttachments = { { .pTexture = &color, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        dependencies.get());
    commands.setViewport(0, 0, (float)color.getWidth(), (float)color.getHeight());
    commands.setScissor(0, 0, color.getWidth(), color.getHeight());
    commands.setRootConstants(constants);
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
    return color;
}
