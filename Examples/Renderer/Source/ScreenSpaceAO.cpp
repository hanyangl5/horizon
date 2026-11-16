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
    AOPipeline = Context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "CSMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .Name = "Renderer.SSAO" });
    BlurPipeline = Context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "BlurMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .Name = "Renderer.SSAO.Blur" });
    RotationSampler = Context.createSampler({ .AddressU = AddressModeRepeat, .AddressV = AddressModeRepeat });
    ASSERT(AOPipeline.isValid() && BlurPipeline.isValid() && RotationSampler.isValid());
}

bool ScreenSpaceAO::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    ASSERT(NewWidth && NewHeight);
    if (!Rotations.isValid())
    {
        // Lossless RGBA8 conversion of Cookbook's data/rot_texture.bmp, with its original 4x4 texels.
        Rotations = Context.loadTexture(DirectoryShaderSources, "../Assets/rotations.dds");
        if (!Rotations.isValid())
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
    Occlusion = Context.createTexture(Desc);
    for (hz::GPUTexture& Texture : Blur)
    {
        Texture = Context.createTexture(Desc);
    }
    Blurred = false;
    ASSERT(Occlusion.isValid() && Blur[0].isValid() && Blur[1].isValid());
    return true;
}

const hz::GPUTexture& ScreenSpaceAO::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                                             const hz::GPUSampler& Sampler)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ScreenSpaceAO::execute");
    ASSERT(Width && Height && Frame.isValid() && Depth.isValid() && Sampler.isValid());
    // The Cookbook kernel and bilateral blur use Depth; the deferred pass multiplies the result into indirect light.
    hz::DependencyList        Dependencies;
    const ScreenSpaceAOConstants Constants = { .frameIndex = Dependencies.read(Frame),
                                               .depthIndex = Dependencies.sample(&Depth),
                                               .rotationsIndex = Dependencies.sample(&Rotations),
                                               .outputIndex = Dependencies.store(&Occlusion),
                                               .samplerIndex = Sampler.getIndex(),
                                               .rotationSamplerIndex = RotationSampler.getIndex(),
                                               .radius = Settings.Radius,
                                               .attenuationScale = Settings.AttenuationScale,
                                               .distanceScale = Settings.DistanceScale };
    Commands.beginGpuTimestamp("SSAO");
    Commands.setPipeline(AOPipeline);
    Commands.setRootConstants(Constants);
    Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1, Dependencies.get());
    const hz::GPUTexture* Source = &Occlusion;
    Blurred = false;
    Commands.setPipeline(BlurPipeline);
    for (uint32_t Pass = 0; Pass < Settings.BlurPasses; ++Pass)
    {
        for (uint32_t Axis = 0; Axis < 2; ++Axis)
        {
            hz::GPUTexture&              Target = Blur[Axis];
            hz::DependencyList           BlurDependencies;
            const ScreenSpaceAOConstants BlurConstants = { .depthIndex = BlurDependencies.sample(&Depth),
                                                           .sourceIndex = BlurDependencies.sample(Source),
                                                           .outputIndex = BlurDependencies.store(&Target),
                                                           .samplerIndex = Sampler.getIndex(),
                                                           .horizontal = Axis == 0,
                                                           .depthThreshold = Settings.DepthThreshold };
            Commands.setRootConstants(BlurConstants);
            Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1, BlurDependencies.get());
            Source = &Target;
            Blurred = true;
        }
    }
    Commands.endGpuTimestamp();
    return GetOcclusion();
}

const hz::GPUTexture& ScreenSpaceAO::GetOcclusion() const { return Blurred ? Blur[1] : Occlusion; }
