// Adapted from Cookbook Renderer/src/scene/Skybox.h; see ../Cookbook-LICENSE.txt.
#include "EnvironmentPass.h"
#include "Profiler/IProfiler.h"

#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>

EnvironmentPass::EnvironmentPass(hz::RenderContext& Context, hz::Format ColorFormat, hz::Format DepthFormat): Context(Context)
{
    Pipeline = Context.CreateGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .pFileName = "Skybox.slang" },
        .Depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
        .colorTargets = { { .Format = ColorFormat } },
        .depthStencilFormat = DepthFormat,
        .Name = "Renderer.Skybox",
    });
    Sampler = Context.CreateSampler(
        { .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    ASSERT(Pipeline.IsValid() && Sampler.IsValid());
}

bool EnvironmentPass::Load(const EnvironmentFiles& Files)
{
    const char* Paths[] = { Files.Prefiltered, Files.Irradiance, Files.BrdfLut };
    for (const char* Path : Paths)
    {
        if (!Path || !Path[0] || !FileExists(Files.Directory, Path))
        {
            LOGF(Error, "Environment requires prefiltered and irradiance cubemaps plus a BRDF LUT; missing '%s' in '%s'",
                 Path ? Path : "<unset>", GetResourceDirectory(Files.Directory));
            return false;
        }
    }
    hz::GPUTexture Candidate[3];
    for (uint32_t Index = 0; Index < 3; ++Index)
    {
        Candidate[Index] = Context.LoadTexture(Files.Directory, Paths[Index]);
        if (!Candidate[Index].IsValid())
        {
            return false;
        }
        const hz::GPUTexture& Texture = Candidate[Index];
        const bool            Cube = Index != 2;
        const TinyImageFormat Format = (TinyImageFormat)Texture.GetFormat();
        if (Texture.GetArraySize() != (Cube ? 6u : 1u) || (Cube && Texture.GetWidth() != Texture.GetHeight()) ||
            (Index == 0 && Texture.GetMipLevels() < 2) || !TinyImageFormat_IsFloat(Format) || TinyImageFormat_HasDepthOrStencil(Format) ||
            TinyImageFormat_IsSRGB(Format))
        {
            LOGF(Error, "Invalid environment texture '%s': expected linear floating-point %s%s", Paths[Index],
                 Cube ? "cubemap" : "2D BRDF LUT", Index == 0 ? " with prefiltered mip levels" : "");
            return false;
        }
    }
    PrefilteredMipCount = Candidate[0].GetMipLevels();
    Prefiltered = std::move(Candidate[0]);
    Irradiance = std::move(Candidate[1]);
    BrdfLut = std::move(Candidate[2]);
    return true;
}

bool EnvironmentPass::IsValid() const
{
    return Pipeline.IsValid() && Sampler.IsValid() && Prefiltered.IsValid() && Irradiance.IsValid() && BrdfLut.IsValid();
}

// Mirrors the RootConstant0 block of Skybox.slang field for field.
struct SkyboxConstants
{
    uint32_t FrameIndex;
    uint32_t EnvironmentIndex;
    uint32_t SamplerIndex;
    float    Intensity;
    Vector4  Tint;
    float2   Rotation;
};
static_assert(sizeof(SkyboxConstants) == 12 * sizeof(uint32_t));

void EnvironmentPass::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Color,
                              const hz::GPUTexture& Depth, float Intensity, const Vector4& Tint, const hz::GPUTexture& Sky,
                              float2 Rotation) const
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("EnvironmentPass::execute");
    ASSERT(IsValid());
    hz::DependencyList    Dependencies;
    const SkyboxConstants Constants = { .FrameIndex = Dependencies.Read(Frame),
                                        .EnvironmentIndex = Dependencies.Sample(&Sky),
                                        .SamplerIndex = Sampler.GetIndex(),
                                        .Intensity = Intensity,
                                        .Tint = Tint,
                                        .Rotation = Rotation };
    Commands.BeginGpuTimestamp("Skybox");
    Commands.BeginRendering(
        { .colorAttachments = { { .pTexture = &Color, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } },
          .depthAttachment = { .pTexture = &Depth, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } },
        Dependencies.Get());
    Commands.SetViewport(0, 0, (float)Context.GetWidth(), (float)Context.GetHeight());
    Commands.SetScissor(0, 0, Context.GetWidth(), Context.GetHeight());
    Commands.SetPipeline(Pipeline);
    Commands.SetRootConstants(Constants);
    Commands.Draw(36);
    Commands.EndRendering();
    Commands.EndGpuTimestamp();
}
