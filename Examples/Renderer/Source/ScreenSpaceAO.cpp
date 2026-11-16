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

bool ScreenSpaceAO::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    ASSERT(NewWidth && NewHeight);
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
    Width = NewWidth;
    Height = NewHeight;
    const hz::TextureDesc Desc = { .Width = Width,
                                   .Height = Height,
                                   .Format = hz::Format::R8G8B8A8_UNORM,
                                   .StartState = ResourceStateUnorderedAccess,
                                   .Descriptors = (DescriptorType)(DescriptorTypeTexture | DescriptorTypeRwTexture),
                                   .Flags = TextureCreationFlagForce2D,
                                   .Name = "Renderer.SSAO" };
    Occlusion = Context.CreateTexture(Desc);
    for (hz::GPUTexture& Texture : Blur)
    {
        Texture = Context.CreateTexture(Desc);
    }
    Blurred = false;
    ASSERT(Occlusion.IsValid() && Blur[0].IsValid() && Blur[1].IsValid());
    return true;
}

const hz::GPUTexture& ScreenSpaceAO::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                                             const hz::GPUSampler& Sampler)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ScreenSpaceAO::execute");
    ASSERT(Width && Height && Frame.IsValid() && Depth.IsValid() && Sampler.IsValid());
    // The Cookbook kernel and bilateral blur use Depth; the deferred pass multiplies the result into indirect light.
    hz::DependencyList        Dependencies;
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
    const hz::GPUTexture* Source = &Occlusion;
    Blurred = false;
    Commands.SetPipeline(BlurPipeline);
    for (uint32_t Pass = 0; Pass < Settings.BlurPasses; ++Pass)
    {
        for (uint32_t Axis = 0; Axis < 2; ++Axis)
        {
            hz::GPUTexture&              Target = Blur[Axis];
            hz::DependencyList           BlurDependencies;
            const ScreenSpaceAOConstants BlurConstants = { .depthIndex = BlurDependencies.sample(&Depth),
                                                           .sourceIndex = BlurDependencies.sample(Source),
                                                           .outputIndex = BlurDependencies.store(&Target),
                                                           .samplerIndex = Sampler.GetIndex(),
                                                           .horizontal = Axis == 0,
                                                           .depthThreshold = Settings.DepthThreshold };
            Commands.SetRootConstants(BlurConstants);
            Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1, BlurDependencies.Get());
            Source = &Target;
            Blurred = true;
        }
    }
    Commands.EndGpuTimestamp();
    return GetOcclusion();
}

const hz::GPUTexture& ScreenSpaceAO::GetOcclusion() const { return Blurred ? Blur[1] : Occlusion; }
