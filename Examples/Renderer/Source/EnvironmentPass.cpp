// Adapted from Cookbook Renderer/src/scene/Skybox.h; see ../Cookbook-LICENSE.txt.
#include "EnvironmentPass.h"
#include "Profiler/IProfiler.h"

#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>

EnvironmentPass::EnvironmentPass(hz::RenderContext& Context, hz::Format ColorFormat, hz::Format DepthFormat): Context(Context)
{
    Pipeline = Context.createGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .pFileName = "Skybox.slang" },
        .depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
        .colorTargets = { { .format = ColorFormat } },
        .depthStencilFormat = DepthFormat,
        .pName = "Renderer.Skybox",
    });
    Sampler = Context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    ASSERT(Pipeline.isValid() && Sampler.isValid());
}

bool EnvironmentPass::Load(const EnvironmentFiles& Files)
{
    const char* Paths[] = { Files.Prefiltered, Files.Irradiance, Files.BrdfLut };
    for (const char* Path : Paths)
    {
        if (!Path || !Path[0] || !fsFileExist(Files.Directory, Path))
        {
            LOGF(eERROR, "Environment requires prefiltered and irradiance cubemaps plus a BRDF LUT; missing '%s' in '%s'",
                 Path ? Path : "<unset>", fsGetResourceDirectory(Files.Directory));
            return false;
        }
    }
    hz::GPUTexture Candidate[3];
    for (uint32_t Index = 0; Index < 3; ++Index)
    {
        Candidate[Index] = Context.loadTexture(Files.Directory, Paths[Index]);
        if (!Candidate[Index].isValid())
        {
            return false;
        }
        const hz::GPUTexture& Texture = Candidate[Index];
        const bool            Cube = Index != 2;
        const TinyImageFormat Format = (TinyImageFormat)Texture.getFormat();
        if (Texture.getArraySize() != (Cube ? 6u : 1u) || (Cube && Texture.getWidth() != Texture.getHeight()) ||
            (Index == 0 && Texture.getMipLevels() < 2) || !TinyImageFormat_IsFloat(Format) || TinyImageFormat_HasDepthOrStencil(Format) ||
            TinyImageFormat_IsSRGB(Format))
        {
            LOGF(eERROR, "Invalid environment texture '%s': expected linear floating-point %s%s", Paths[Index],
                 Cube ? "cubemap" : "2D BRDF LUT", Index == 0 ? " with prefiltered mip levels" : "");
            return false;
        }
    }
    PrefilteredMipCount = Candidate[0].getMipLevels();
    Prefiltered = std::move(Candidate[0]);
    Irradiance = std::move(Candidate[1]);
    BrdfLut = std::move(Candidate[2]);
    return true;
}

bool EnvironmentPass::IsValid() const
{
    return Pipeline.isValid() && Sampler.isValid() && Prefiltered.isValid() && Irradiance.isValid() && BrdfLut.isValid();
}

// Mirrors the RootConstant0 block of Skybox.slang field for field.
struct SkyboxConstants
{
    uint32_t frameIndex;
    uint32_t environmentIndex;
    uint32_t samplerIndex;
    float    intensity;
};
static_assert(sizeof(SkyboxConstants) == 3 * sizeof(uint32_t) + sizeof(float));

void EnvironmentPass::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Color,
                              const hz::GPUTexture& Depth, float Intensity) const
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("EnvironmentPass::Execute");
    ASSERT(IsValid());
    hz::DependencyList    Dependencies;
    const SkyboxConstants Constants = { .frameIndex = Dependencies.read(Frame),
                                        .environmentIndex = Dependencies.sample(&Prefiltered),
                                        .samplerIndex = Sampler.getIndex(),
                                        .intensity = Intensity };
    Commands.beginGpuTimestamp("Skybox");
    Commands.beginRendering(
        { .colorAttachments = { { .pTexture = &Color, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } },
          .depthAttachment = { .pTexture = &Depth, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } },
        Dependencies.get());
    Commands.setViewport(0, 0, (float)Context.getWidth(), (float)Context.getHeight());
    Commands.setScissor(0, 0, Context.getWidth(), Context.getHeight());
    Commands.setPipeline(Pipeline);
    Commands.setRootConstants(Constants);
    Commands.draw(36);
    Commands.endRendering();
    Commands.endGpuTimestamp();
}
