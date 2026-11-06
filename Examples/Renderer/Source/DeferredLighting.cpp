// Adapted from Cookbook lighting.frag.slang; see ../Cookbook-LICENSE.txt.
#include "DeferredLighting.h"

static uint32_t lightingTextureIndex(const hz::GPUTexture* texture)
{
    return texture && texture->isValid() ? texture->getSrvIndex() : UINT32_MAX;
}
static uint32_t lightingSamplerIndex(const hz::GPUSampler* sampler)
{
    return sampler && sampler->isValid() ? sampler->getIndex() : UINT32_MAX;
}

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
                                                const hz::SceneFrameSnapshot& scene, const LightingResources& resources)
{
    ASSERT(scene.isValid() && color.isValid() && resources.pLightingFrame && resources.pLightingFrame->isValid());
    ASSERT(gbuffer.pEmissive && gbuffer.pNormalMaterial && gbuffer.pBaseColorMetallic && gbuffer.pDepth);
    ASSERT(resources.pEnvironment && resources.pIrradiance && resources.pBRDFLut && resources.pEnvironmentSampler);
    const hz::GPUTexture* sampled[10];
    uint32_t              count = 0;
    const hz::GPUTexture* inputs[] = { gbuffer.pEmissive,     gbuffer.pNormalMaterial, gbuffer.pBaseColorMetallic, gbuffer.pDepth,
                                       resources.pShadowMap,  resources.pRTShadowAO,   resources.pRTReflection,    resources.pEnvironment,
                                       resources.pIrradiance, resources.pBRDFLut };
    for (const hz::GPUTexture* texture : inputs)
    {
        if (texture && texture->isValid())
        {
            sampled[count++] = texture;
        }
    }
    commands.beginGpuTimestamp("DeferredLighting");
    // Keep graphics state active during GBV attachment-transition checks after DXR.
    commands.setPipeline(pipeline);
    commands.beginRendering(
        { .colorAttachments = { { .pTexture = &color, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        { .sampledTextures = { sampled, count }, .buffers = { &frame, resources.pLightingFrame } });
    commands.setViewport(0, 0, (float)color.getWidth(), (float)color.getHeight());
    commands.setScissor(0, 0, color.getWidth(), color.getHeight());
    const uint32_t constants[] = { frame.getSrvIndex(),
                                   resources.pLightingFrame->getSrvIndex(),
                                   gbuffer.pEmissive->getSrvIndex(),
                                   gbuffer.pNormalMaterial->getSrvIndex(),
                                   gbuffer.pBaseColorMetallic->getSrvIndex(),
                                   gbuffer.pDepth->getSrvIndex(),
                                   lightingTextureIndex(resources.pShadowMap),
                                   lightingSamplerIndex(resources.pShadowSampler),
                                   lightingTextureIndex(resources.pEnvironment),
                                   lightingTextureIndex(resources.pIrradiance),
                                   lightingTextureIndex(resources.pBRDFLut),
                                   lightingSamplerIndex(resources.pEnvironmentSampler),
                                   lightingTextureIndex(resources.pRTShadowAO),
                                   lightingTextureIndex(resources.pRTReflection) };
    commands.setRootConstant(constants);
    commands.draw(3);
    commands.endRendering();
    commands.endGpuTimestamp();
    return color;
}
