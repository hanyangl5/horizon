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
    uint32_t FrameIndex;
    uint32_t LightingFrameIndex;
    uint32_t EmissiveIndex;
    uint32_t NormalMaterialIndex;
    uint32_t BaseColorMetallicIndex;
    uint32_t DepthIndex;
    uint32_t ShadowMapIndex;
    uint32_t ShadowSamplerIndex;
    uint32_t EnvironmentIndex;
    uint32_t IrradianceIndex;
    uint32_t BrdfLutIndex;
    uint32_t EnvironmentSamplerIndex;
    uint32_t RtShadowAOIndex;
    uint32_t RtReflectionIndex;
    uint32_t GTAOIndex;
};
static_assert(sizeof(DeferredLightingConstants) == 15 * sizeof(uint32_t));

DeferredLighting::DeferredLighting(hz::RenderContext& Context)
{
    Pipeline = Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
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
    hz::DependencyList              Dependencies;
    if (Resources.Lights)
    {
        Dependencies.Read(*Resources.Lights);
        Dependencies.Read(*Resources.ActiveLights);
    }
    const DeferredLightingConstants Constants = {
        .FrameIndex = Dependencies.Read(Frame),
        .LightingFrameIndex = Dependencies.Read(*Resources.LightingFrame),
        .EmissiveIndex = Dependencies.Sample(GBuffer.Emissive),
        .NormalMaterialIndex = Dependencies.Sample(GBuffer.NormalMaterial),
        .BaseColorMetallicIndex = Dependencies.Sample(GBuffer.BaseColorMetallic),
        .DepthIndex = Dependencies.Sample(GBuffer.Depth),
        .ShadowMapIndex = Dependencies.Sample(Resources.ShadowMap),
        .ShadowSamplerIndex = LightingSamplerIndex(Resources.ShadowSampler),
        .EnvironmentIndex = Dependencies.Sample(Resources.Environment),
        .IrradianceIndex = Dependencies.Sample(Resources.Irradiance),
        .BrdfLutIndex = Dependencies.Sample(Resources.BRDFLut),
        .EnvironmentSamplerIndex = LightingSamplerIndex(Resources.EnvironmentSampler),
        .RtShadowAOIndex = Dependencies.Sample(Resources.RTShadowAO),
        .RtReflectionIndex = Dependencies.Sample(Resources.RTReflection),
        .GTAOIndex = Dependencies.Sample(Resources.GTAO),
    };
    Commands.BeginGpuTimestamp("DeferredLighting");
    // Keep graphics state active during GBV attachment-transition checks after DXR.
    Commands.SetPipeline(Pipeline);
    Commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &Color, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        Dependencies.Get());
    Commands.SetViewport(0, 0, (float)Color.GetWidth(), (float)Color.GetHeight());
    Commands.SetScissor(0, 0, Color.GetWidth(), Color.GetHeight());
    Commands.SetRootConstants(Constants);
    Commands.Draw(3);
    Commands.EndRendering();
    Commands.EndGpuTimestamp();
}
