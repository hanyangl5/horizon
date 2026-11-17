// ReSTIR DI resource layer; the sampling and reuse stages live in RestirDI.slang. See Docs/RestirDI.md.
#include "RestirDI.h"
#include "Profiler/IProfiler.h"

#include <math.h>

// Same daylight-white calibration ShadowMap applies to the sun: 100 lm/W.
static constexpr float kLightEfficacy = 100.0f;

static uint32_t LightingSamplerIndex(const hz::GPUSampler* Sampler)
{
    return Sampler && Sampler->IsValid() ? Sampler->GetIndex() : UINT32_MAX;
}

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
    uint32_t colorIndex;
    uint32_t rtShadowAOIndex;
    uint32_t rtReflectionIndex;
    uint32_t ssaoIndex;
    uint32_t width;
    uint32_t height;
};
static_assert(sizeof(RestirDIConstants) == 23 * sizeof(uint32_t));

RestirDI::RestirDI(hz::RenderContext& Context): Context(Context)
{
    Pipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "RestirDI.slang" },
          .Name = "Renderer.RestirDI" });
    ASSERT(Pipeline.IsValid());
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
    for (hz::GPUBuffer& Buffer : Reservoirs)
    {
        Buffer = Context.CreateBuffer({ .Size = (uint64_t)Width * Height * sizeof(RestirDIReservoir),
                                        .ElementCount = Width * Height,
                                        .StructStride = sizeof(RestirDIReservoir),
                                        .Name = "Renderer.RestirDI.Reservoir",
                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                        .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER) });
        ASSERT(Buffer.IsValid());
    }
    // Dimensions changed: the old history describes pixels that no longer exist.
    ClearReservoirs = true;
}

void RestirDI::UploadLights(const hz::SceneView& View)
{
    LightCount = 0;
    const hz::Span<hz::RenderLight> Sources = View.Scene.GetLights();
    for (uint32_t Index : View.Scene.GetActiveLights())
    {
        const hz::RenderLight& Source = Sources.Data[Index];
        if (!Source.ready)
        {
            continue;
        }
        if (LightCount >= MaxLights)
        {
            LOGF(Warning, "ReSTIR DI truncates the scene light table at %u lights", MaxLights);
            break;
        }
        const float   Intensity = Source.intensity * exp2f(Source.exposure) * kLightEfficacy;
        const Vector3 Position = Source.transform.getCol3().getXYZ();
        const Vector3 Forward = -normalize(Source.transform.getCol2().getXYZ());
        RestirDILight& Light = LightUpload[LightCount++];
        Light = { .position = Vector4(Position, Source.range),
                  .direction = Vector4(Forward, cosf(Source.outerConeAngle)),
                  .color = Vector4(Source.color[0] * Intensity, Source.color[1] * Intensity, Source.color[2] * Intensity,
                                   cosf(Source.innerConeAngle)),
                  .type = (uint32_t)Source.Type,
                  .width = 0,
                  .height = 0,
                  .area = 0 };
        switch (Source.Type)
        {
        case hz::LightType::Rect:
            Light.width = Source.Width;
            Light.height = Source.Height;
            Light.area = Source.Width * Source.Height;
            break;
        case hz::LightType::Disk:
            Light.width = Source.radius;
            Light.area = 3.14159265359f * Source.radius * Source.radius;
            break;
        case hz::LightType::Sphere:
            Light.width = Source.radius;
            Light.area = 4.0f * 3.14159265359f * Source.radius * Source.radius;
            break;
        case hz::LightType::Cylinder:
            Light.width = Source.radius;
            Light.height = Source.length;
            Light.area = 2.0f * 3.14159265359f * Source.radius * Source.length;
            break;
        default:
            break;
        }
    }
    if (!LightCount)
    {
        return;
    }
    if (!LoggedLights)
    {
        LOGF(Info, "ReSTIR DI samples %u lights", LightCount);
        LoggedLights = true;
    }
    // The buffer is persistent-mapped and per frame in flight, so the copy happens now rather than in the
    // command list: earlier submissions were still reading their own copy.
    const bool Updated = Context.UpdateBuffer(Lights[View.FrameIndex], 0, LightUpload, LightCount * sizeof(RestirDILight));
    ASSERT(Updated);
}

void RestirDI::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const hz::SceneView& View,
                       const LightingResources& Resources, const hz::GPUTexture& Color)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RestirDI::Execute");
    ASSERT(Color.IsValid() && Width && Resources.LightingFrame && Resources.LightingFrame->IsValid());
    ASSERT(GBuffer.Emissive && GBuffer.NormalMaterial && GBuffer.BaseColorMetallic && GBuffer.MotionMaterialId && GBuffer.Depth);
    ASSERT(Resources.Environment && Resources.Irradiance && Resources.BRDFLut && Resources.EnvironmentSampler);
    UploadLights(View);

    const uint32_t Current = View.FrameIndex % ReservoirCount;
    const uint32_t Previous = (Current + ReservoirCount - 1) % ReservoirCount;
    hz::DependencyList Dependencies;
    const RestirDIConstants Constants = {
        .frameIndex = Dependencies.read(Frame),
        .lightingFrameIndex = Dependencies.read(*Resources.LightingFrame),
        .lightsIndex = Dependencies.read(Lights[View.FrameIndex]),
        .lightCount = LightCount,
        .emissiveIndex = Dependencies.sample(GBuffer.Emissive),
        .normalMaterialIndex = Dependencies.sample(GBuffer.NormalMaterial),
        .baseColorMetallicIndex = Dependencies.sample(GBuffer.BaseColorMetallic),
        .depthIndex = Dependencies.sample(GBuffer.Depth),
        .motionIndex = Dependencies.sample(GBuffer.MotionMaterialId),
        .shadowMapIndex = Dependencies.sample(Resources.ShadowMap),
        .shadowSamplerIndex = LightingSamplerIndex(Resources.ShadowSampler),
        .environmentIndex = Dependencies.sample(Resources.Environment),
        .irradianceIndex = Dependencies.sample(Resources.Irradiance),
        .brdfLutIndex = Dependencies.sample(Resources.BRDFLut),
        .environmentSamplerIndex = LightingSamplerIndex(Resources.EnvironmentSampler),
        .previousReservoirIndex = Dependencies.read(Reservoirs[Previous]),
        .outputReservoirIndex = Dependencies.write(Reservoirs[Current]),
        .colorIndex = Dependencies.store(&Color),
        .rtShadowAOIndex = Dependencies.sample(Resources.RTShadowAO),
        .rtReflectionIndex = Dependencies.sample(Resources.RTReflection),
        .ssaoIndex = Dependencies.sample(Resources.SSAO),
        .width = Width,
        .height = Height,
    };

    Commands.BeginGpuTimestamp("RestirDI");
    if (ClearReservoirs)
    {
        for (hz::GPUBuffer& Buffer : Reservoirs)
        {
            Commands.FillBuffer(Buffer, 0, 0, Buffer.GetSize());
        }
        ClearReservoirs = false;
    }
    Commands.SetPipeline(Pipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 7) / 8, (Height + 7) / 8, 1, Dependencies.Get());
    Commands.EndGpuTimestamp();
}
