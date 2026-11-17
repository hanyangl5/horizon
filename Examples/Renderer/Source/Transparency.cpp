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
static_assert(sizeof(TransparencyConstants) == 28 * sizeof(uint32_t));

Transparency::Transparency(hz::RenderContext& Context, hz::Format DepthFormat)
{
    ClearPipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "ClearMain" } }, .pFileName = "Transparency.slang" },
          .Name = "Renderer.OIT.Clear" });
    GatherPipeline =
        Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = ShaderStageVert, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = ShaderStageFrag, .pEntryPoint = "PSTransparent" } },
                                                         .pFileName = "Transparency.slang" },
                                         .Depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = DepthFormat,
                                         .indirectDrawId = true,
                                         .Name = "Renderer.OIT.Gather" });
    ResolvePipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = ShaderStageComp, .pEntryPoint = "ResolveMain" } }, .pFileName = "Transparency.slang" },
          .Name = "Renderer.OIT.Resolve" });
    SurfaceSampler = Context.CreateSampler();
    ASSERT(ClearPipeline.IsValid() && GatherPipeline.IsValid() && ResolvePipeline.IsValid() && SurfaceSampler.IsValid());
}

void Transparency::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth, const SceneView& View,
                           const GpuDrawList& Draws, const LightingResources& Lighting, const hz::GPUTexture& OpaqueColor,
                           const TransparencyTargets& Targets)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Transparency::execute");
    ASSERT(Targets.Heads && Targets.Color && Targets.Nodes && Targets.Counters && Lighting.LightingFrame && Lighting.EnvironmentSampler);
    const hz::GPUTexture& Heads = *Targets.Heads;
    const hz::GPUTexture& Color = *Targets.Color;
    const hz::GPUBuffer&  Nodes = *Targets.Nodes;
    const hz::GPUBuffer&  Counters = *Targets.Counters;
    const uint32_t        Width = Color.GetWidth();
    const uint32_t        Height = Color.GetHeight();
    const uint32_t        MaxNodes = (uint32_t)(Nodes.GetSize() / NodeBytes);
    TransparencyConstants Constants = { .frameIndex = Frame.GetSrvIndex(),
                                        .oitHeadsIndex = Heads.GetUavIndex(),
                                        .oitCounterIndex = Counters.GetUavIndex(),
                                        .oitNodesIndex = Nodes.GetUavIndex(),
                                        .oitMaxNodes = MaxNodes,
                                        .lightingFrameIndex = Lighting.LightingFrame->GetSrvIndex() };
    Commands.BeginGpuTimestamp("OIT");
    Commands.SetPipeline(ClearPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1, { .storageTextures = { &Heads }, .storageBuffers = { &Counters } });
    const uint32_t Backing = View.FrameIndex;
    if (Draws.GetCount(Backing))
    {
        const hz::GPUBuffer& Indirect = Draws.GetCommands(Backing, Draws.GetTransparentList());
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
            if (Texture && Texture->IsValid())
            {
                Sampled.PushBack(Texture);
            }
        }
        const hz::GPUTexture* StorageTextures[] = { &Heads };
        const hz::GPUBuffer*  StorageBuffers[] = { &Counters, &Nodes };
        Dependencies.sampledTextures = { Sampled.GetData(), Sampled.Size() };
        Dependencies.buffers = { Buffers.GetData(), Buffers.Size() };
        Dependencies.storageTextures = StorageTextures;
        Dependencies.storageBuffers = StorageBuffers;
        Commands.BeginRendering(
            { .depthAttachment = { .pTexture = &Depth, .loadAction = LoadActionLoad, .storeAction = StoreActionStore } }, Dependencies);
        Commands.SetViewport(0, 0, (float)Width, (float)Height);
        Commands.SetScissor(0, 0, Width, Height);
        Commands.SetPipeline(GatherPipeline);
        const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
        Constants.instancesIndex = Gpu.instances.GetSrvIndex();
        Constants.materialsIndex = Gpu.materials.GetSrvIndex();
        Constants.texturesIndex = Gpu.textures.GetSrvIndex();
        Constants.samplerIndex = SurfaceSampler.GetIndex();
        Constants.drawsIndex = Draws.GetDrawData(Backing).GetSrvIndex();
        Constants.shadowMapIndex = Lighting.ShadowMap ? Lighting.ShadowMap->GetSrvIndex() : UINT32_MAX;
        Constants.shadowSamplerIndex = Lighting.ShadowSampler ? Lighting.ShadowSampler->GetIndex() : UINT32_MAX;
        Constants.environmentIndex = Lighting.Environment ? Lighting.Environment->GetSrvIndex() : UINT32_MAX;
        Constants.irradianceIndex = Lighting.Irradiance ? Lighting.Irradiance->GetSrvIndex() : UINT32_MAX;
        Constants.brdfLutIndex = Lighting.BRDFLut ? Lighting.BRDFLut->GetSrvIndex() : UINT32_MAX;
        Constants.environmentSamplerIndex = Lighting.EnvironmentSampler->GetIndex();
        Commands.SetRootConstants(Constants);
        Commands.DrawIndirect(Indirect, 0, Draws.GetCount(Backing));
        Commands.EndRendering();
    }
    Constants.oitHeadsIndex = Heads.GetSrvIndex();
    Constants.oitNodesIndex = Nodes.GetSrvIndex();
    Constants.oitOpaqueIndex = OpaqueColor.GetSrvIndex();
    Constants.oitOpacityBoost = OpacityBoost;
    Constants.oitShowHeatmap = ShowHeatmap ? 1u : 0u;
    Constants.oitOutputIndex = Color.GetUavIndex();
    Constants.oitTime = float(double(GetUSec(true)) * 1e-6);
    Commands.SetPipeline(ResolvePipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 7) / 8, (Height + 7) / 8, 1,
                      { .sampledTextures = { &Heads, &OpaqueColor },
                        .storageTextures = { &Color },
                        .buffers = { &Nodes },
                        .storageBuffers = { &Counters } });
    Commands.EndGpuTimestamp();
}
