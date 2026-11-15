// Adapted from Cookbook OITPass / transparent.frag.slang / oit.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Transparency.h"
#include "Profiler/IProfiler.h"
#include "Core/ITime.h"

// Mirrors the RootConstant0 block shared by the Transparency.slang entry points.
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

Transparency::Transparency(hz::RenderContext& Context, hz::Format DepthFormat): Context(Context)
{
    ClearPipeline = Context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearMain" } }, .pFileName = "Transparency.slang" },
          .pName = "Renderer.OIT.Clear" });
    GatherPipeline =
        Context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSTransparent" } },
                                                         .pFileName = "Transparency.slang" },
                                         .depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = DepthFormat,
                                         .indirectDrawId = true,
                                         .pName = "Renderer.OIT.Gather" });
    ResolvePipeline = Context.createComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ResolveMain" } }, .pFileName = "Transparency.slang" },
          .pName = "Renderer.OIT.Resolve" });
    SurfaceSampler = Context.createSampler();
    ASSERT(ClearPipeline.isValid() && GatherPipeline.isValid() && ResolvePipeline.isValid() && SurfaceSampler.isValid());
}

void Transparency::Resize(uint32_t NewWidth, uint32_t NewHeight, uint32_t FragmentsPerPixel)
{
    ASSERT(NewWidth && NewHeight && FragmentsPerPixel);
    Width = NewWidth;
    Height = NewHeight;
    const uint64_t Capacity = uint64_t(Width) * Height * FragmentsPerPixel;
    ASSERT(Capacity <= UINT32_MAX / 16);
    MaxNodes = (uint32_t)Capacity;
    Heads = Context.createTexture({ .width = Width,
                                    .height = Height,
                                    .format = hz::Format::R32_UINT,
                                    .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                    .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                    .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                    .pName = "Renderer.OIT.Heads" });
    Color = Context.createTexture({ .width = Width,
                                    .height = Height,
                                    .format = hz::Format::R32G32B32A32_SFLOAT,
                                    .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                    .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                    .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                    .pName = "Renderer.OIT.Resolved" });
    Nodes = Context.createBuffer({ .size = Capacity * 16,
                                   .elementCount = MaxNodes * 4,
                                   .pName = "Renderer.OIT.Nodes",
                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
    Counters = Context.createBuffer({ .size = 16,
                                      .elementCount = 4,
                                      .pName = "Renderer.OIT.Counters",
                                      .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                      .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                      .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
    ASSERT(Heads.isValid() && Color.isValid() && Nodes.isValid() && Counters.isValid());
}

const hz::GPUTexture& Transparency::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                                            const SceneView& View, const GpuDrawList& Draws, const LightingResources& Lighting,
                                            const hz::GPUTexture& OpaqueColor)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Transparency::Execute");
    ASSERT(Width && Height && Lighting.LightingFrame && Lighting.EnvironmentSampler);
    TransparencyConstants Constants = { .frameIndex = Frame.getSrvIndex(),
                                        .oitHeadsIndex = Heads.getUavIndex(),
                                        .oitCounterIndex = Counters.getUavIndex(),
                                        .oitNodesIndex = Nodes.getUavIndex(),
                                        .oitMaxNodes = MaxNodes,
                                        .lightingFrameIndex = Lighting.LightingFrame->getSrvIndex() };
    Commands.beginGpuTimestamp("OIT");
    Commands.setPipeline(ClearPipeline);
    Commands.setRootConstants(Constants);
    Commands.dispatch((Width + 15) / 16, (Height + 15) / 16, 1, { .storageTextures = { &Heads }, .storageBuffers = { &Counters } });
    const uint32_t Backing = View.FrameIndex;
    if (Draws.GetCount(Backing))
    {
        const hz::GPUBuffer& Indirect = Draws.GetCommands(Backing, true);
        hz::Dependencies     Dependencies = Draws.GetDependencies(Frame, View, &Indirect);
        Sampled.Clear();
        Buffers.Clear();
        for (const hz::GPUTexture* Texture : Dependencies.sampledTextures)
        {
            Sampled.PushBack(Texture);
        }
        for (const hz::GPUBuffer* Buffer : Dependencies.buffers)
        {
            Buffers.PushBack(Buffer);
        }
        Buffers.PushBack(Lighting.LightingFrame);
        const hz::GPUTexture* LightingTextures[] = { Lighting.ShadowMap, Lighting.Environment, Lighting.Irradiance, Lighting.BRDFLut };
        for (const hz::GPUTexture* Texture : LightingTextures)
        {
            if (Texture && Texture->isValid())
            {
                Sampled.PushBack(Texture);
            }
        }
        const hz::GPUTexture* StorageTextures[] = { &Heads };
        const hz::GPUBuffer*  StorageBuffers[] = { &Counters, &Nodes };
        Dependencies.sampledTextures = { Sampled.data(), Sampled.size() };
        Dependencies.buffers = { Buffers.data(), Buffers.size() };
        Dependencies.storageTextures = StorageTextures;
        Dependencies.storageBuffers = StorageBuffers;
        Commands.beginRendering(
            { .depthAttachment = { .pTexture = &Depth, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } }, Dependencies);
        Commands.setViewport(0, 0, (float)Width, (float)Height);
        Commands.setScissor(0, 0, Width, Height);
        Commands.setPipeline(GatherPipeline);
        const hz::GpuSceneBuffers& Gpu = View.GpuScene.getBuffers();
        Constants.instancesIndex = Gpu.instances.getSrvIndex();
        Constants.materialsIndex = Gpu.materials.getSrvIndex();
        Constants.texturesIndex = Gpu.textures.getSrvIndex();
        Constants.samplerIndex = SurfaceSampler.getIndex();
        Constants.drawsIndex = Draws.GetDrawData(Backing).getSrvIndex();
        Constants.shadowMapIndex = Lighting.ShadowMap ? Lighting.ShadowMap->getSrvIndex() : UINT32_MAX;
        Constants.shadowSamplerIndex = Lighting.ShadowSampler ? Lighting.ShadowSampler->getIndex() : UINT32_MAX;
        Constants.environmentIndex = Lighting.Environment ? Lighting.Environment->getSrvIndex() : UINT32_MAX;
        Constants.irradianceIndex = Lighting.Irradiance ? Lighting.Irradiance->getSrvIndex() : UINT32_MAX;
        Constants.brdfLutIndex = Lighting.BRDFLut ? Lighting.BRDFLut->getSrvIndex() : UINT32_MAX;
        Constants.environmentSamplerIndex = Lighting.EnvironmentSampler->getIndex();
        Commands.setRootConstants(Constants);
        Commands.drawIndirect(Indirect, 0, Draws.GetCount(Backing));
        Commands.endRendering();
    }
    Constants.oitHeadsIndex = Heads.getSrvIndex();
    Constants.oitNodesIndex = Nodes.getSrvIndex();
    Constants.oitOpaqueIndex = OpaqueColor.getSrvIndex();
    Constants.oitOpacityBoost = OpacityBoost;
    Constants.oitShowHeatmap = ShowHeatmap ? 1u : 0u;
    Constants.oitOutputIndex = Color.getUavIndex();
    Constants.oitTime = float(double(GetUSec(true)) * 1e-6);
    Commands.setPipeline(ResolvePipeline);
    Commands.setRootConstants(Constants);
    Commands.dispatch((Width + 7) / 8, (Height + 7) / 8, 1,
                      { .sampledTextures = { &Heads, &OpaqueColor },
                        .storageTextures = { &Color },
                        .buffers = { &Nodes },
                        .storageBuffers = { &Counters } });
    Commands.endGpuTimestamp();
    return Color;
}
