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
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .pName = "Renderer.SSAO" });
    BlurPipeline = Context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "BlurMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .pName = "Renderer.SSAO.Blur" });
    RotationSampler = Context.createSampler({ .addressU = ADDRESS_MODE_REPEAT, .addressV = ADDRESS_MODE_REPEAT });
    ASSERT(AOPipeline.isValid() && BlurPipeline.isValid() && RotationSampler.isValid());
}

bool ScreenSpaceAO::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    ASSERT(NewWidth && NewHeight);
    if (!Rotations.isValid())
    {
        // Lossless RGBA8 conversion of Cookbook's data/rot_texture.bmp, with its original 4x4 texels.
        Rotations = Context.loadTexture(RD_SHADER_SOURCES, "../Assets/rotations.dds");
        if (!Rotations.isValid())
        {
            LOGF(eERROR, "SSAO requires Examples/Renderer/Assets/rotations.dds (Cookbook rotation texture)");
            return false;
        }
    }
    Width = NewWidth;
    Height = NewHeight;
    const hz::TextureDesc Desc = { .width = Width,
                                   .height = Height,
                                   .format = hz::Format::R8G8B8A8_UNORM,
                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                   .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                   .pName = "Renderer.SSAO" };
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
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ScreenSpaceAO::Execute");
    ASSERT(Width && Height && Frame.isValid() && Depth.isValid() && Sampler.isValid());
    // The Cookbook kernel and bilateral blur use depth; the deferred pass multiplies the result into indirect light.
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
