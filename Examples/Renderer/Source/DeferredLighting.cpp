// Adapted from Cookbook lighting.frag.slang; see ../Cookbook-LICENSE.txt.
#include "DeferredLighting.h"
#include "Profiler/IProfiler.h"

static uint32_t LightingSamplerIndex(const hz::GPUSampler* Sampler)
{
    return Sampler && Sampler->isValid() ? Sampler->getIndex() : UINT32_MAX;
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

DeferredLighting::DeferredLighting(hz::RenderContext& Context): Context(Context)
{
    Pipeline = Context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                           { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                               .pFileName = "DeferredLighting.slang" },
                                                .colorTargets = { { .format = hz::Format::R32G32B32A32_SFLOAT } },
                                                .pName = "Renderer.DeferredLighting" });
    ASSERT(Pipeline.isValid());
}
void DeferredLighting::Resize(uint32_t Width, uint32_t Height)
{
    Color = Context.createTexture({ .width = Width,
                                    .height = Height,
                                    .format = hz::Format::R32G32B32A32_SFLOAT,
                                    .startState = RESOURCE_STATE_RENDER_TARGET,
                                    .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                    .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                    .renderTarget = true,
                                    .pName = "Renderer.DeferredLighting.Color" });
    ASSERT(Color.isValid());
}
const hz::GPUTexture& DeferredLighting::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                                                const SceneView& View, const LightingResources& Resources)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("DeferredLighting::Execute");
    ASSERT(Color.isValid() && Resources.LightingFrame && Resources.LightingFrame->isValid());
    ASSERT(GBuffer.Emissive && GBuffer.NormalMaterial && GBuffer.BaseColorMetallic && GBuffer.Depth);
    ASSERT(Resources.Environment && Resources.Irradiance && Resources.BRDFLut && Resources.EnvironmentSampler);
    hz::DependencyList Dependencies;
    const DeferredLightingConstants Constants = {
        .frameIndex = Dependencies.read(Frame),
        .lightingFrameIndex = Dependencies.read(*Resources.LightingFrame),
        .emissiveIndex = Dependencies.sample(GBuffer.Emissive),
        .normalMaterialIndex = Dependencies.sample(GBuffer.NormalMaterial),
        .baseColorMetallicIndex = Dependencies.sample(GBuffer.BaseColorMetallic),
        .depthIndex = Dependencies.sample(GBuffer.Depth),
        .shadowMapIndex = Dependencies.sample(Resources.ShadowMap),
        .shadowSamplerIndex = LightingSamplerIndex(Resources.ShadowSampler),
        .environmentIndex = Dependencies.sample(Resources.Environment),
        .irradianceIndex = Dependencies.sample(Resources.Irradiance),
        .brdfLutIndex = Dependencies.sample(Resources.BRDFLut),
        .environmentSamplerIndex = LightingSamplerIndex(Resources.EnvironmentSampler),
        .rtShadowAOIndex = Dependencies.sample(Resources.RTShadowAO),
        .rtReflectionIndex = Dependencies.sample(Resources.RTReflection),
        .ssaoIndex = Dependencies.sample(Resources.SSAO),
    };
    Commands.beginGpuTimestamp("DeferredLighting");
    // Keep graphics state active during GBV attachment-transition checks after DXR.
    Commands.setPipeline(Pipeline);
    Commands.beginRendering(
        { .colorAttachments = { { .pTexture = &Color, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        Dependencies.get());
    Commands.setViewport(0, 0, (float)Color.getWidth(), (float)Color.getHeight());
    Commands.setScissor(0, 0, Color.getWidth(), Color.getHeight());
    Commands.setRootConstants(Constants);
    Commands.draw(3);
    Commands.endRendering();
    Commands.endGpuTimestamp();
    return Color;
}
