// ReSTIR DI lighting pass; the sampling and reuse stages live in RestirDI.slang. See Docs/RestirDI.md.
#include "RestirDI.h"
#include "Profiler/IProfiler.h"
#include "Scene/GpuDrawList.h"

#include <math.h>

// Same daylight-white calibration ShadowMap applies to the sun: 100 lm/W.
static constexpr float kLightEfficacy = 100.0f;

// Mirrors the RootConstant0 block of RestirDI.slang field for field.
struct RestirDIConstants
{
    uint32_t frameIndex;
    uint32_t lightingFrameIndex;
    uint32_t lightsIndex;
    uint32_t lightCount;
    uint32_t emissiveIndex;
    uint32_t normalMaterialIndex;
    uint32_t baseColorMetallicIndex;
    uint32_t depthIndex;
    uint32_t motionIndex;
    uint32_t shadowMapIndex;
    uint32_t shadowSamplerIndex;
    uint32_t environmentIndex;
    uint32_t irradianceIndex;
    uint32_t brdfLutIndex;
    uint32_t environmentSamplerIndex;
    uint32_t previousReservoirIndex;
    uint32_t outputReservoirIndex;
    uint32_t currentReservoirIndex;
    uint32_t colorIndex;
    uint32_t rtShadowAOIndex;
    uint32_t rtReflectionIndex;
    uint32_t ssaoIndex;
    uint32_t tlasIndex;
    uint32_t drawsIndex;
    uint32_t materialsIndex;
    uint32_t samplerIndex;
    uint32_t width;
    uint32_t height;
    uint32_t frameNumber;
    uint32_t initialCandidates;
    uint32_t temporalReuse;
    uint32_t spatialSamples;
    float    spatialRadius;
    uint32_t maxHistory;
    uint32_t reference;
};
static_assert(sizeof(RestirDIConstants) == 35 * sizeof(uint32_t));

static uint32_t SamplerIndex(const hz::GPUSampler* Sampler) { return Sampler && Sampler->IsValid() ? Sampler->GetIndex() : UINT32_MAX; }

RestirDI::RestirDI(hz::RenderContext& Context): Context(Context)
{
    CandidatesPipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSCandidates" } }, .pFileName = "RestirDI.slang" },
          .Name = "Renderer.RestirDI.Candidates" });
    ShadePipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSShade" } }, .pFileName = "RestirDI.slang" },
          .Name = "Renderer.RestirDI.Shade" });
    MaterialSampler = Context.CreateSampler();
    ASSERT(CandidatesPipeline.IsValid() && ShadePipeline.IsValid() && MaterialSampler.IsValid());
    // One light table per frame in flight, so a submitted frame keeps reading the lights it sampled.
    for (hz::GPUBuffer& Buffer : Lights)
    {
        Buffer = Context.CreateBuffer({ .Size = MaxLights * sizeof(RestirDILight),
                                        .ElementCount = MaxLights,
                                        .StructStride = sizeof(RestirDILight),
                                        .Name = "Renderer.RestirDI.Lights",
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .Descriptors = DESCRIPTOR_TYPE_BUFFER,
                                        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(Buffer.IsValid());
    }
}

void RestirDI::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    Width = NewWidth;
    Height = NewHeight;
    const hz::BufferDesc Desc = { .Size = (uint64_t)Width * Height * sizeof(RestirDIReservoir),
                                  .ElementCount = Width * Height,
                                  .StructStride = sizeof(RestirDIReservoir),
                                  .Name = "Renderer.RestirDI.Reservoirs",
                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                  .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER) };
    for (hz::GPUBuffer& Buffer : History)
    {
        Buffer = Context.CreateBuffer(Desc);
        ASSERT(Buffer.IsValid());
    }
    ClearHistory = true;
}

void RestirDI::UploadLights(const hz::SceneView& View)
{
    LightCount = 0;
    const hz::Span<hz::RenderLight> Sources = View.Scene.GetLights();
    for (uint32_t Index : View.Scene.GetActiveLights())
    {
        const hz::RenderLight& Source = Sources.Data[Index];
        // The sun stays on the analytic path; the other area emitters are not sampled yet.
        if (!Source.ready || (Source.Type != hz::LightType::Point && Source.Type != hz::LightType::Spot))
        {
            continue;
        }
        if (LightCount >= MaxLights)
        {
            LOGF(Warning, "ReSTIR DI truncates the scene light table at %u lights", MaxLights);
            break;
        }
        const float Intensity = Source.intensity * exp2f(Source.exposure) * kLightEfficacy;
        LightUpload[LightCount++] = {
            .position = Vector4(Source.transform.getCol3().getXYZ(), Source.range),
            .direction = Vector4(-normalize(Source.transform.getCol2().getXYZ()), cosf(Source.outerConeAngle)),
            .color =
                Vector4(Source.color[0] * Intensity, Source.color[1] * Intensity, Source.color[2] * Intensity, cosf(Source.innerConeAngle)),
            .type = (uint32_t)Source.Type,
        };
    }
    if (!LoggedLights && LightCount)
    {
        LOGF(Info, "ReSTIR DI samples %u local lights", LightCount);
        LoggedLights = true;
    }
    if (LightCount)
    {
        // Persistent-mapped and per frame in flight, so earlier submissions keep reading their own copy.
        const bool Updated = Context.UpdateBuffer(Lights[View.FrameIndex], 0, LightUpload, LightCount * sizeof(RestirDILight));
        ASSERT(Updated);
    }
}

void RestirDI::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const hz::SceneView& View,
                       const hz::GpuDrawList& Draws, const hz::GPUAccelerationStructure* TLAS, const LightingResources& Resources,
                       const hz::GPUTexture& Color)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RestirDI::Execute");
    ASSERT(Color.IsValid() && Width == Color.GetWidth() && Height == Color.GetHeight() && Resources.LightingFrame);
    ASSERT(GBuffer.Emissive && GBuffer.NormalMaterial && GBuffer.BaseColorMetallic && GBuffer.MotionMaterialId && GBuffer.Depth);
    UploadLights(View);
    if (!(Settings == PreviousSettings))
    {
        ClearHistory = true;
        PreviousSettings = Settings;
    }

    const hz::GPUBuffer& Previous = History[CurrentHistory ^ 1u];
    const hz::GPUBuffer& Output = History[CurrentHistory];
    // Resource indices are filled in below as each dispatch declares its dependencies.
    RestirDIConstants    Constants = {
           .lightCount = LightCount,
           .shadowSamplerIndex = SamplerIndex(Resources.ShadowSampler),
           .environmentSamplerIndex = SamplerIndex(Resources.EnvironmentSampler),
           .tlasIndex = TLAS ? TLAS->GetSrvIndex() : UINT32_MAX,
           .drawsIndex = Draws.GetDrawData(View.FrameIndex).GetSrvIndex(),
           .materialsIndex = View.GpuScene.GetBuffers().materials.GetSrvIndex(),
           .samplerIndex = MaterialSampler.GetIndex(),
           .width = Width,
           .height = Height,
           .frameNumber = FrameNumber++,
           .initialCandidates = TF_MAX(Settings.InitialCandidates, 1u),
           .temporalReuse = Settings.TemporalReuse && !ClearHistory ? 1u : 0u,
           .spatialSamples = Settings.SpatialSamples,
           .spatialRadius = Settings.SpatialRadius,
           .maxHistory = Settings.MaxHistory,
           .reference = Settings.Reference ? 1u : 0u,
    };

    Commands.BeginGpuTimestamp("ReSTIR DI");
    hz::DependencyList Candidate;
    Constants.lightingFrameIndex = Candidate.read(*Resources.LightingFrame);
    Constants.frameIndex = Candidate.read(Frame);
    Constants.lightsIndex = Candidate.read(Lights[View.FrameIndex]);
    Constants.normalMaterialIndex = Candidate.sample(GBuffer.NormalMaterial);
    Constants.baseColorMetallicIndex = Candidate.sample(GBuffer.BaseColorMetallic);
    Constants.depthIndex = Candidate.sample(GBuffer.Depth);
    Constants.motionIndex = Candidate.sample(GBuffer.MotionMaterialId);
    Constants.previousReservoirIndex = Candidate.read(Previous);
    Constants.outputReservoirIndex = Candidate.write(Output);
    Commands.SetPipeline(CandidatesPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 7) / 8, (Height + 7) / 8, 1, Candidate.Get());

    // The shading pass reads the draw and material tables for alpha-tested shadow rays.
    const hz::Dependencies Scene = Draws.GetDependencies(Frame, View);
    hz::DependencyList     Shade;
    Shade.read(*Resources.LightingFrame);
    Shade.read(Lights[View.FrameIndex]);
    Constants.emissiveIndex = Shade.sample(GBuffer.Emissive);
    Shade.sample(GBuffer.NormalMaterial);
    Shade.sample(GBuffer.BaseColorMetallic);
    Shade.sample(GBuffer.Depth);
    Shade.sample(GBuffer.MotionMaterialId);
    Constants.shadowMapIndex = Shade.sample(Resources.ShadowMap);
    Constants.environmentIndex = Shade.sample(Resources.Environment);
    Constants.irradianceIndex = Shade.sample(Resources.Irradiance);
    Constants.brdfLutIndex = Shade.sample(Resources.BRDFLut);
    Constants.rtShadowAOIndex = Shade.sample(Resources.RTShadowAO);
    Constants.rtReflectionIndex = Shade.sample(Resources.RTReflection);
    Constants.ssaoIndex = Shade.sample(Resources.SSAO);
    Constants.colorIndex = Shade.store(&Color);
    Constants.currentReservoirIndex = Shade.read(Output);
    hz::Dependencies ShadeDependencies = Shade.Get();
    ShadeDependencies.pShared = &Scene;
    Commands.SetPipeline(ShadePipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 7) / 8, (Height + 7) / 8, 1, ShadeDependencies);
    Commands.EndGpuTimestamp();

    CurrentHistory ^= 1u;
    ClearHistory = false;
}
