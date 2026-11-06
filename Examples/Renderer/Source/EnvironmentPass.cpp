// Adapted from Cookbook Renderer/src/scene/Skybox.h; see ../Cookbook-LICENSE.txt.
#include "EnvironmentPass.h"
#include "Profiler/IProfiler.h"

#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"
#include <ThirdParty/tinyimageformat/tinyimageformat_query.h>

EnvironmentPass::EnvironmentPass(hz::RenderContext& context, hz::Format colorFormat, hz::Format depthFormat): context(context)
{
    pipeline = context.createGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .pFileName = "Skybox.slang" },
        .depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
        .colorTargets = { { .format = colorFormat } },
        .depthStencilFormat = depthFormat,
        .pName = "Renderer.Skybox",
    });
    sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    ASSERT(pipeline.isValid() && sampler.isValid());
}

bool EnvironmentPass::load(const EnvironmentFiles& files)
{
    const char* paths[] = { files.prefiltered, files.irradiance, files.brdfLut };
    for (const char* path : paths)
    {
        if (!path || !path[0] || !fsFileExist(files.directory, path))
        {
            LOGF(eERROR, "Environment requires prefiltered and irradiance cubemaps plus a BRDF LUT; missing '%s' in '%s'",
                 path ? path : "<unset>", fsGetResourceDirectory(files.directory));
            return false;
        }
    }
    hz::GPUTexture candidate[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        candidate[i] = context.loadTexture(files.directory, paths[i]);
        if (!candidate[i].isValid())
        {
            return false;
        }
        const hz::GPUTexture& texture = candidate[i];
        const bool            cube = i != 2;
        const TinyImageFormat format = (TinyImageFormat)texture.getFormat();
        if (texture.getArraySize() != (cube ? 6u : 1u) || (cube && texture.getWidth() != texture.getHeight()) ||
            (i == 0 && texture.getMipLevels() < 2) || !TinyImageFormat_IsFloat(format) || TinyImageFormat_HasDepthOrStencil(format) ||
            TinyImageFormat_IsSRGB(format))
        {
            LOGF(eERROR, "Invalid environment texture '%s': expected linear floating-point %s%s", paths[i],
                 cube ? "cubemap" : "2D BRDF LUT", i == 0 ? " with prefiltered mip levels" : "");
            return false;
        }
    }
    prefilteredMipCount = candidate[0].getMipLevels();
    prefiltered = std::move(candidate[0]);
    irradiance = std::move(candidate[1]);
    brdfLut = std::move(candidate[2]);
    return true;
}

bool EnvironmentPass::isValid() const
{
    return pipeline.isValid() && sampler.isValid() && prefiltered.isValid() && irradiance.isValid() && brdfLut.isValid();
}

void EnvironmentPass::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& color,
                              const hz::GPUTexture& depth, float intensity) const
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("EnvironmentPass::execute");
    ASSERT(isValid());
    commands.beginGpuTimestamp("Skybox");
    commands.beginRendering(
        { .colorAttachments = { { .pTexture = &color, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } },
          .depthAttachment = { .pTexture = &depth, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } },
        { .sampledTextures = { &prefiltered }, .buffers = { &frame } });
    commands.setViewport(0, 0, (float)context.getWidth(), (float)context.getHeight());
    commands.setScissor(0, 0, context.getWidth(), context.getHeight());
    commands.setPipeline(pipeline);
    const struct
    {
        uint32_t frame;
        uint32_t prefiltered;
        uint32_t sampler;
        float    intensity;
    } constants = { .frame = frame.getSrvIndex(),
                    .prefiltered = prefiltered.getSrvIndex(),
                    .sampler = sampler.getIndex(),
                    .intensity = intensity };
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / sizeof(uint32_t) });
    commands.draw(36);
    commands.endRendering();
    commands.endGpuTimestamp();
}
