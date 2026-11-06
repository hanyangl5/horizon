// Adapted from Cookbook OITPass / transparent.frag.slang / oit.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Transparency.h"
#include "Profiler/IProfiler.h"
#include "Core/ITime.h"

struct TransparencyConstants
{
    uint32_t instanceIndex = 0;
    uint32_t materialIndex = 0;
    uint32_t materialId = 0;
    uint32_t frameIndex = UINT32_MAX;
    uint32_t instancesIndex = UINT32_MAX;
    uint32_t materialsIndex = UINT32_MAX;
    uint32_t texturesIndex = UINT32_MAX;
    uint32_t samplerIndex = UINT32_MAX;
    uint32_t positionsIndex = UINT32_MAX;
    uint32_t normalsIndex = UINT32_MAX;
    uint32_t texcoordsIndex = UINT32_MAX;
    uint32_t drawsIndex = UINT32_MAX;
    uint32_t oitHeadsIndex = UINT32_MAX;
    uint32_t oitCounterIndex = UINT32_MAX;
    uint32_t oitNodesIndex = UINT32_MAX;
    uint32_t oitMaxNodes = 0;
    uint32_t lightingFrameIndex = UINT32_MAX;
    uint32_t shadowMapIndex = UINT32_MAX;
    uint32_t shadowSamplerIndex = UINT32_MAX;
    uint32_t environmentIndex = UINT32_MAX;
    uint32_t irradianceIndex = UINT32_MAX;
    uint32_t brdfLutIndex = UINT32_MAX;
    uint32_t environmentSamplerIndex = UINT32_MAX;
    uint32_t oitOpaqueIndex = UINT32_MAX;
    float    oitOpacityBoost = 0;
    uint32_t oitShowHeatmap = 0;
    uint32_t oitOutputIndex = UINT32_MAX;
    float    oitTime = 0;
};

Transparency::Transparency(hz::RenderContext& context, hz::Format depthFormat): context(context)
{
    clearPipeline = context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearMain" } }, .pFileName = "Transparency.slang" },
          .pName = "Renderer.OIT.Clear" });
    gatherPipeline =
        context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSTransparent" } },
                                                         .pFileName = "Transparency.slang" },
                                         .depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = depthFormat,
                                         .indirectDrawId = true,
                                         .pName = "Renderer.OIT.Gather" });
    resolvePipeline = context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ResolveMain" } }, .pFileName = "Transparency.slang" },
          .pName = "Renderer.OIT.Resolve" });
    surfaceSampler = context.createSampler();
    ASSERT(clearPipeline.isValid() && gatherPipeline.isValid() && resolvePipeline.isValid() && surfaceSampler.isValid());
}
void Transparency::resize(uint32_t newWidth, uint32_t newHeight, uint32_t fragmentsPerPixel)
{
    ASSERT(newWidth && newHeight && fragmentsPerPixel);
    width = newWidth;
    height = newHeight;
    const uint64_t capacity = uint64_t(width) * height * fragmentsPerPixel;
    ASSERT(capacity <= UINT32_MAX / 16);
    maxNodes = (uint32_t)capacity;
    heads = context.createTexture({ .width = width,
                                    .height = height,
                                    .format = hz::Format::R32_UINT,
                                    .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                    .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                    .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                    .pName = "Renderer.OIT.Heads" });
    color = context.createTexture({ .width = width,
                                    .height = height,
                                    .format = hz::Format::R32G32B32A32_SFLOAT,
                                    .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                    .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                    .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                    .pName = "Renderer.OIT.Resolved" });
    nodes = context.createBuffer({ .size = capacity * 16,
                                   .elementCount = maxNodes * 4,
                                   .pName = "Renderer.OIT.Nodes",
                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
    counters = context.createBuffer({ .size = 16,
                                      .elementCount = 4,
                                      .pName = "Renderer.OIT.Counters",
                                      .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                      .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                      .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
    ASSERT(heads.isValid() && color.isValid() && nodes.isValid() && counters.isValid());
}
const hz::GPUTexture& Transparency::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const hz::GPUTexture& depth,
                                            const SceneView& view, const GpuDrawList& draws, const LightingResources& lighting,
                                            const hz::GPUTexture& opaqueColor)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Transparency::execute");
    ASSERT(width && height && lighting.pLightingFrame && lighting.pEnvironmentSampler);
    TransparencyConstants constants = { .frameIndex = frame.getSrvIndex(),
                                        .oitHeadsIndex = heads.getUavIndex(),
                                        .oitCounterIndex = counters.getUavIndex(),
                                        .oitNodesIndex = nodes.getUavIndex(),
                                        .oitMaxNodes = maxNodes,
                                        .lightingFrameIndex = lighting.pLightingFrame->getSrvIndex() };
    commands.beginGpuTimestamp("OIT");
    commands.setPipeline(clearPipeline);
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1, { .storageTextures = { &heads }, .storageBuffers = { &counters } });
    const uint32_t backing = view.frameIndex;
    if (draws.getCount(backing))
    {
        const hz::GPUBuffer& indirect = draws.getCommands(backing, true);
        hz::Dependencies     dependencies = draws.getDependencies(frame, view, indirect);
        sampled.clear();
        buffers.clear();
        for (const hz::GPUTexture* texture : dependencies.sampledTextures)
        {
            sampled.pushBack(texture);
        }
        for (const hz::GPUBuffer* buffer : dependencies.buffers)
        {
            buffers.pushBack(buffer);
        }
        buffers.pushBack(lighting.pLightingFrame);
        const hz::GPUTexture* lightingTextures[] = { lighting.pShadowMap, lighting.pEnvironment, lighting.pIrradiance, lighting.pBRDFLut };
        for (const hz::GPUTexture* texture : lightingTextures)
        {
            if (texture && texture->isValid())
            {
                sampled.pushBack(texture);
            }
        }
        const hz::GPUTexture* storageTextures[] = { &heads };
        const hz::GPUBuffer*  storageBuffers[] = { &counters, &nodes };
        dependencies.sampledTextures = { sampled.data(), sampled.size() };
        dependencies.buffers = { buffers.data(), buffers.size() };
        dependencies.storageTextures = storageTextures;
        dependencies.storageBuffers = storageBuffers;
        commands.beginRendering(
            { .depthAttachment = { .pTexture = &depth, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } }, dependencies);
        commands.setViewport(0, 0, (float)width, (float)height);
        commands.setScissor(0, 0, width, height);
        commands.setPipeline(gatherPipeline);
        const hz::GpuSceneBuffers& gpu = view.gpu.getBuffers();
        constants.instancesIndex = gpu.instances.getSrvIndex();
        constants.materialsIndex = gpu.materials.getSrvIndex();
        constants.texturesIndex = gpu.textures.getSrvIndex();
        constants.samplerIndex = surfaceSampler.getIndex();
        constants.drawsIndex = draws.getDrawData(backing).getSrvIndex();
        constants.shadowMapIndex = lighting.pShadowMap ? lighting.pShadowMap->getSrvIndex() : UINT32_MAX;
        constants.shadowSamplerIndex = lighting.pShadowSampler ? lighting.pShadowSampler->getIndex() : UINT32_MAX;
        constants.environmentIndex = lighting.pEnvironment ? lighting.pEnvironment->getSrvIndex() : UINT32_MAX;
        constants.irradianceIndex = lighting.pIrradiance ? lighting.pIrradiance->getSrvIndex() : UINT32_MAX;
        constants.brdfLutIndex = lighting.pBRDFLut ? lighting.pBRDFLut->getSrvIndex() : UINT32_MAX;
        constants.environmentSamplerIndex = lighting.pEnvironmentSampler->getIndex();
        commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / 4 });
        commands.drawIndirect(indirect, 0, draws.getCount(backing));
        commands.endRendering();
    }
    constants.oitHeadsIndex = heads.getSrvIndex();
    constants.oitNodesIndex = nodes.getSrvIndex();
    constants.oitOpaqueIndex = opaqueColor.getSrvIndex();
    constants.oitOpacityBoost = opacityBoost;
    constants.oitShowHeatmap = showHeatmap ? 1u : 0u;
    constants.oitOutputIndex = color.getUavIndex();
    constants.oitTime = float(double(getUSec(true)) * 1e-6);
    commands.setPipeline(resolvePipeline);
    commands.setRootConstant({ (const uint32_t*)&constants, sizeof(constants) / 4 });
    commands.dispatch((width + 7) / 8, (height + 7) / 8, 1,
                      { .sampledTextures = { &heads, &opaqueColor },
                        .storageTextures = { &color },
                        .buffers = { &nodes },
                        .storageBuffers = { &counters } });
    commands.endGpuTimestamp();
    return color;
}
