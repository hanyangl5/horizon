// Adapted from Cookbook lighting.frag.slang; see ../Cookbook-LICENSE.txt.
#include "DeferredLighting.h"
#include "Profiler/IProfiler.h"

static uint32_t LightingSamplerIndex(const hz::GPUSampler* Sampler)
{
    return Sampler && Sampler->IsValid() ? Sampler->GetIndex() : UINT32_MAX;
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

DeferredLighting::DeferredLighting(hz::RenderContext& Context)
{
    Pipeline = Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSMain" },
                                                                           { .stage = ShaderStageFrag, .pEntryPoint = "PSMain" } },
                                                               .pFileName = "DeferredLighting.slang" },
                                                .colorTargets = { { .Format = ColorFormat } },
                                                .Name = "Renderer.DeferredLighting" });
    ASSERT(Pipeline.IsValid());
}
void DeferredLighting::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                               const LightingResources& Resources, const hz::GPUTexture& Color)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("DeferredLighting::Execute");
    ASSERT(Color.IsValid() && Resources.LightingFrame && Resources.LightingFrame->IsValid());
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
    Commands.BeginGpuTimestamp("DeferredLighting");
    // Keep graphics state active during GBV attachment-transition checks after DXR.
    Commands.SetPipeline(Pipeline);
    Commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &Color, .loadAction = LoadActionDontCare, .storeAction = StoreActionStore } } },
        Dependencies.Get());
    Commands.SetViewport(0, 0, (float)Color.GetWidth(), (float)Color.GetHeight());
    Commands.SetScissor(0, 0, Color.GetWidth(), Color.GetHeight());
    Commands.SetRootConstants(Constants);
    Commands.Draw(3);
    Commands.EndRendering();
    Commands.EndGpuTimestamp();
}
