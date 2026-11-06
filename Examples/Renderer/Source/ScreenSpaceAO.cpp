#include "ScreenSpaceAO.h"
#include "Profiler/IProfiler.h"

#include "Core/ILog.h"

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
    uint32_t occlusionIndex = UINT32_MAX;
    float    radius = 0;
    float    attenuationScale = 1;
    float    distanceScale = 1;
    float    depthThreshold = 0;
    float    scale = 1;
    float    bias = 0;
};

ScreenSpaceAO::ScreenSpaceAO(hz::RenderContext& context): context(context)
{
    aoPipeline = context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .pName = "Renderer.SSAO" });
    blurPipeline = context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "BlurMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .pName = "Renderer.SSAO.Blur" });
    combinePipeline = context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CombineMain" } }, .pFileName = "ScreenSpaceAO.slang" },
          .pName = "Renderer.SSAO.Combine" });
    rotationSampler = context.createSampler({ .addressU = ADDRESS_MODE_REPEAT, .addressV = ADDRESS_MODE_REPEAT });
    ASSERT(aoPipeline.isValid() && blurPipeline.isValid() && combinePipeline.isValid() && rotationSampler.isValid());
}

bool ScreenSpaceAO::resize(uint32_t newWidth, uint32_t newHeight)
{
    ASSERT(newWidth && newHeight);
    if (!rotations.isValid())
    {
        // Lossless RGBA8 conversion of Cookbook's data/rot_texture.bmp, with its original 4x4 texels.
        rotations = context.loadTexture(RD_SHADER_SOURCES, "../Assets/rotations.dds");
        if (!rotations.isValid())
        {
            LOGF(eERROR, "SSAO requires Examples/Renderer/Assets/rotations.dds (Cookbook rotation texture)");
            return false;
        }
    }
    width = newWidth;
    height = newHeight;
    const hz::TextureDesc desc = { .width = width,
                                   .height = height,
                                   .format = hz::Format::R8G8B8A8_UNORM,
                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                   .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                   .pName = "Renderer.SSAO" };
    occlusion = context.createTexture(desc);
    for (hz::GPUTexture& texture : blur)
    {
        texture = context.createTexture(desc);
    }
    hz::TextureDesc colorDesc = desc;
    colorDesc.format = hz::Format::R32G32B32A32_SFLOAT;
    colorDesc.pName = "Renderer.SSAO.CombinedColor";
    combined = context.createTexture(colorDesc);
    blurred = false;
    ASSERT(occlusion.isValid() && blur[0].isValid() && blur[1].isValid() && combined.isValid());
    return true;
}

const hz::GPUTexture& ScreenSpaceAO::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& depth,
                                             const hz::GPUTexture& normal, const hz::GPUSampler& sampler)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ScreenSpaceAO::execute");
    ASSERT(width && height && frame.isValid() && depth.isValid() && normal.isValid() && sampler.isValid());
    // The Cookbook kernel and bilateral blur use depth, rather than a second normal-based AO algorithm.
    const ScreenSpaceAOConstants constants = { .frameIndex = frame.getSrvIndex(),
                                               .depthIndex = depth.getSrvIndex(),
                                               .rotationsIndex = rotations.getSrvIndex(),
                                               .outputIndex = occlusion.getUavIndex(),
                                               .samplerIndex = sampler.getIndex(),
                                               .rotationSamplerIndex = rotationSampler.getIndex(),
                                               .radius = settings.radius,
                                               .attenuationScale = settings.attenuationScale,
                                               .distanceScale = settings.distanceScale };
    commands.beginGpuTimestamp("SSAO");
    commands.setPipeline(aoPipeline);
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { &depth, &rotations }, .storageTextures = { &occlusion }, .buffers = { &frame } });
    const hz::GPUTexture* source = &occlusion;
    blurred = false;
    commands.setPipeline(blurPipeline);
    for (uint32_t pass = 0; pass < settings.blurPasses; ++pass)
    {
        for (uint32_t axis = 0; axis < 2; ++axis)
        {
            hz::GPUTexture&              target = blur[axis];
            const ScreenSpaceAOConstants blurConstants = { .depthIndex = depth.getSrvIndex(),
                                                           .sourceIndex = source->getSrvIndex(),
                                                           .outputIndex = target.getUavIndex(),
                                                           .samplerIndex = sampler.getIndex(),
                                                           .horizontal = axis == 0,
                                                           .depthThreshold = settings.depthThreshold };
            commands.setRootConstant({ (const uint32_t*)&blurConstants, sizeof(blurConstants) / sizeof(uint32_t) });
            commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                              { .sampledTextures = { &depth, source }, .storageTextures = { &target } });
            source = &target;
            blurred = true;
        }
    }
    commands.endGpuTimestamp();
    return getOcclusion();
}

const hz::GPUTexture& ScreenSpaceAO::combine(hz::CommandList& commands, const hz::GPUTexture& color, const hz::GPUSampler& sampler)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ScreenSpaceAO::combine");
    ASSERT(width && height && color.isValid() && sampler.isValid());
    const hz::GPUTexture&        ao = getOcclusion();
    const ScreenSpaceAOConstants constants = { .sourceIndex = color.getSrvIndex(),
                                               .outputIndex = combined.getUavIndex(),
                                               .samplerIndex = sampler.getIndex(),
                                               .occlusionIndex = ao.getSrvIndex(),
                                               .scale = settings.scale,
                                               .bias = settings.bias };
    commands.beginGpuTimestamp("SSAO.Combine");
    commands.setPipeline(combinePipeline);
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1, { .sampledTextures = { &color, &ao }, .storageTextures = { &combined } });
    commands.endGpuTimestamp();
    return combined;
}

const hz::GPUTexture& ScreenSpaceAO::getOcclusion() const { return blurred ? blur[1] : occlusion; }
