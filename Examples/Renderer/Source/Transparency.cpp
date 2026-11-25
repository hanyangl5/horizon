// Adapted from Cookbook OITPass / transparent.frag.slang / oit.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Transparency.h"
#include "Profiler/IProfiler.h"
#include "Core/ITime.h"

// Mirrors the RootConstant0 block shared by the Transparency.slang entry points.
struct TransparencyConstants
{
    uint32_t InstanceIndex = 0;
    uint32_t MaterialIndex = 0;
    uint32_t MaterialId = 0;
    uint32_t FrameIndex = UINT32_MAX;
    uint32_t InstancesIndex = UINT32_MAX;
    uint32_t MaterialsIndex = UINT32_MAX;
    uint32_t TexturesIndex = UINT32_MAX;
    uint32_t SamplerIndex = UINT32_MAX;
    uint32_t PositionsIndex = UINT32_MAX;
    uint32_t NormalsIndex = UINT32_MAX;
    uint32_t TexcoordsIndex = UINT32_MAX;
    uint32_t DrawsIndex = UINT32_MAX;
    uint32_t OitHeadsIndex = UINT32_MAX;
    uint32_t OitCounterIndex = UINT32_MAX;
    uint32_t OitNodesIndex = UINT32_MAX;
    uint32_t OitMaxNodes = 0;
    uint32_t LightingFrameIndex = UINT32_MAX;
    uint32_t ShadowMapIndex = UINT32_MAX;
    uint32_t ShadowSamplerIndex = UINT32_MAX;
    uint32_t EnvironmentIndex = UINT32_MAX;
    uint32_t IrradianceIndex = UINT32_MAX;
    uint32_t BrdfLutIndex = UINT32_MAX;
    uint32_t EnvironmentSamplerIndex = UINT32_MAX;
    uint32_t OitOpaqueIndex = UINT32_MAX;
    float    OitOpacityBoost = 0;
    uint32_t OitShowHeatmap = 0;
    uint32_t OitOutputIndex = UINT32_MAX;
    float    OitTime = 0;
};
static_assert(sizeof(TransparencyConstants) == 28 * sizeof(uint32_t));

Transparency::Transparency(hz::RenderContext& Context, hz::Format DepthFormat)
{
    ClearPipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearMain" } }, .pFileName = "Transparency.slang" },
          .Name = "Renderer.OIT.Clear" });
    GatherPipeline =
        Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSTransparent" } },
                                                         .pFileName = "Transparency.slang" },
                                         .Depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = DepthFormat,
                                         .indirectDrawId = true,
                                         .Name = "Renderer.OIT.Gather" });
    ResolvePipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ResolveMain" } }, .pFileName = "Transparency.slang" },
          .Name = "Renderer.OIT.Resolve" });
    SurfaceSampler = Context.CreateSampler();
    ASSERT(ClearPipeline.IsValid() && GatherPipeline.IsValid() && ResolvePipeline.IsValid() && SurfaceSampler.IsValid());
}

void Transparency::Execute(hz::CommandList& Commands, hz::BufferSrv Frame, const hz::GPUTexture& Depth, const hz::SceneView& View,
                           const hz::GpuDrawList& Draws, const LightingResources& Lighting, const hz::GPUTexture& OpaqueColor,
                           const TransparencyTargets& Targets)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Transparency::execute");
    ASSERT(Targets.Heads && Targets.Color && Targets.Nodes && Targets.Counters && Lighting.LightingFrame.IsValid() &&
           Lighting.EnvironmentSampler);
    const hz::GPUTexture& Heads = *Targets.Heads;
    const hz::GPUTexture& Color = *Targets.Color;
    const hz::GPUBuffer&  Nodes = *Targets.Nodes;
    const hz::GPUBuffer&  Counters = *Targets.Counters;
    const uint32_t        Width = Color.GetWidth();
    const uint32_t        Height = Color.GetHeight();
    const uint32_t        MaxNodes = (uint32_t)(Nodes.GetSize() / NodeBytes);
    TransparencyConstants Constants = { .FrameIndex = Frame.Index,
                                        .OitHeadsIndex = Heads.GetUavIndex(),
                                        .OitCounterIndex = Counters.GetUavIndex(),
                                        .OitNodesIndex = Nodes.GetUavIndex(),
                                        .OitMaxNodes = MaxNodes,
                                        .LightingFrameIndex = Lighting.LightingFrame.Index };
    Commands.BeginGpuTimestamp("OIT");
    Commands.SetPipeline(ClearPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1, { .storageTextures = { &Heads }, .storageBuffers = { &Counters } });
    const uint32_t Backing = View.FrameIndex;
    if (Draws.GetCount(Backing))
    {
        const hz::GPUBuffer& Indirect = Draws.GetCommands(Backing, Draws.GetTransparentList());
        hz::Dependencies     Dependencies = Draws.GetDependencies(*Frame.Buffer, View, &Indirect);
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
        if (Lighting.LightingFrame.Buffer != Frame.Buffer)
        {
            Buffers.PushBack(Lighting.LightingFrame.Buffer);
        }
        if (Lighting.TileLightBitMasks)
        {
            Buffers.PushBack(Lighting.TileLightBitMasks);
        }
        if (Lighting.LocalLights)
        {
            Buffers.PushBack(Lighting.LocalLights);
            Buffers.PushBack(Lighting.ActiveLocalLights);
        }
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
            { .depthAttachment = { .pTexture = &Depth, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } }, Dependencies);
        Commands.SetViewport(0, 0, (float)Width, (float)Height);
        Commands.SetScissor(0, 0, Width, Height);
        Commands.SetPipeline(GatherPipeline);
        const hz::GpuSceneBuffers& Gpu = View.GpuScene.GetBuffers();
        Constants.InstancesIndex = Gpu.instances.GetSrvIndex();
        Constants.MaterialsIndex = Gpu.materials.GetSrvIndex();
        Constants.TexturesIndex = Gpu.textures.GetSrvIndex();
        Constants.SamplerIndex = SurfaceSampler.GetIndex();
        Constants.DrawsIndex = Draws.GetDrawData(Backing).GetSrvIndex();
        Constants.ShadowMapIndex = Lighting.ShadowMap ? Lighting.ShadowMap->GetSrvIndex() : UINT32_MAX;
        Constants.ShadowSamplerIndex = Lighting.ShadowSampler ? Lighting.ShadowSampler->GetIndex() : UINT32_MAX;
        Constants.EnvironmentIndex = Lighting.Environment ? Lighting.Environment->GetSrvIndex() : UINT32_MAX;
        Constants.IrradianceIndex = Lighting.Irradiance ? Lighting.Irradiance->GetSrvIndex() : UINT32_MAX;
        Constants.BrdfLutIndex = Lighting.BRDFLut ? Lighting.BRDFLut->GetSrvIndex() : UINT32_MAX;
        Constants.EnvironmentSamplerIndex = Lighting.EnvironmentSampler->GetIndex();
        Commands.SetRootConstants(Constants);
        Commands.DrawIndirect(Indirect, 0, Draws.GetCount(Backing));
        Commands.EndRendering();
    }
    Constants.OitHeadsIndex = Heads.GetSrvIndex();
    Constants.OitNodesIndex = Nodes.GetSrvIndex();
    Constants.OitOpaqueIndex = OpaqueColor.GetSrvIndex();
    Constants.OitOpacityBoost = OpacityBoost;
    Constants.OitShowHeatmap = ShowHeatmap ? 1u : 0u;
    Constants.OitOutputIndex = Color.GetUavIndex();
    Constants.OitTime = float(double(GetUSec(true)) * 1e-6);
    Commands.SetPipeline(ResolvePipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Width + 7) / 8, (Height + 7) / 8, 1,
                      { .sampledTextures = { &Heads, &OpaqueColor },
                        .storageTextures = { &Color },
                        .buffers = { &Nodes },
                        .storageBuffers = { &Counters } });
    Commands.EndGpuTimestamp();
}
