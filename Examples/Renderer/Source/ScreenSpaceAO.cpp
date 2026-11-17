#include "ScreenSpaceAO.h"
#include "Profiler/IProfiler.h"

#include "Core/ILog.h"

// Mirrors the RootConstant0 block of ScreenSpaceAO.slang field for field.
struct ScreenSpaceAOConstants
{
    uint32_t frameIndex = UINT32_MAX;
    uint32_t depthIndex = UINT32_MAX;
    uint32_t rotationsIndex = UINT32_MAX;
    uint32_t sourceIndex = UINT32_MAX;
    uint32_t outputIndex = UINT32_MAX;
    uint32_t samplerIndex = UINT32_MAX;
    uint32_t rotationSamplerIndex = UINT32_MAX;
    uint32_t horizontal = 0;
    float    radius = 0;
    float    attenuationScale = 1;
    float    distanceScale = 1;
    float    depthThreshold = 0;
};
static_assert(sizeof(ScreenSpaceAOConstants) == 12 * sizeof(uint32_t));

ScreenSpaceAO::ScreenSpaceAO(hz::RenderContext& Context): Context(Context)
{
    AOPipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .Name = "Renderer.SSAO" });
    BlurPipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "BlurMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .Name = "Renderer.SSAO.Blur" });
    RotationSampler = Context.CreateSampler({ .AddressU = AddressModeRepeat, .AddressV = AddressModeRepeat });
    ASSERT(AOPipeline.IsValid() && BlurPipeline.IsValid() && RotationSampler.IsValid());
}

bool ScreenSpaceAO::Load()
{
    if (!Rotations.IsValid())
    {
        // Lossless RGBA8 conversion of Cookbook's data/rot_texture.bmp, with its original 4x4 texels.
        Rotations = Context.LoadTexture(DirectoryShaderSources, "../Assets/rotations.dds");
        if (!Rotations.IsValid())
        {
            LOGF(Error, "SSAO requires Examples/Renderer/Assets/rotations.dds (Cookbook rotation texture)");
            return false;
        }
    }
    return true;
}

void ScreenSpaceAO::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                            const hz::GPUSampler& Sampler, const hz::GPUTexture& Occlusion, const hz::GPUTexture& Scratch)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ScreenSpaceAO::execute");
    ASSERT(Rotations.IsValid() && Frame.IsValid() && Depth.IsValid() && Sampler.IsValid());
    const uint32_t               Width = Occlusion.GetWidth();
    const uint32_t               Height = Occlusion.GetHeight();
    // The Cookbook kernel and bilateral blur use Depth; the deferred pass multiplies the result into indirect light.
    hz::DependencyList           Dependencies;
    const ScreenSpaceAOConstants Constants = { .frameIndex = Dependencies.read(Frame),
                                               .depthIndex = Dependencies.sample(&Depth),
                                               .rotationsIndex = Dependencies.sample(&Rotations),
                                               .outputIndex = Dependencies.store(&Occlusion),
                                               .samplerIndex = Sampler.GetIndex(),
                                               .rotationSamplerIndex = RotationSampler.GetIndex(),
                                               .radius = Settings.Radius,
                                               .attenuationScale = Settings.AttenuationScale,
                                               .distanceScale = Settings.DistanceScale };
    Commands.BeginGpuTimestamp("SSAO");
    Commands.SetPipeline(AOPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1, Dependencies.Get());
    Commands.SetPipeline(BlurPipeline);
    for (uint32_t Pass = 0; Pass < Settings.BlurPasses; ++Pass)
    {
        // Horizontal into Scratch, vertical back, so the result always ends in Occlusion.
        for (uint32_t Axis = 0; Axis < 2; ++Axis)
        {
            const hz::GPUTexture&        Source = Axis == 0 ? Occlusion : Scratch;
            const hz::GPUTexture&        Target = Axis == 0 ? Scratch : Occlusion;
            hz::DependencyList           BlurDependencies;
            const ScreenSpaceAOConstants BlurConstants = { .depthIndex = BlurDependencies.sample(&Depth),
                                                           .sourceIndex = BlurDependencies.sample(&Source),
                                                           .outputIndex = BlurDependencies.store(&Target),
                                                           .samplerIndex = Sampler.GetIndex(),
                                                           .horizontal = Axis == 0,
                                                           .depthThreshold = Settings.DepthThreshold };
            Commands.SetRootConstants(BlurConstants);
            Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1, BlurDependencies.Get());
        }
    }
    Commands.EndGpuTimestamp();
}
