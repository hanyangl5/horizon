#include "Core/ICVar.h"
#include "GTAO.h"

struct GTAOConstants
{
    uint32_t FrameIndex = UINT32_MAX;
    uint32_t DepthIndex = UINT32_MAX;
    uint32_t NormalIndex = UINT32_MAX;
    uint32_t SourceIndex = UINT32_MAX;
    uint32_t OutputIndex = UINT32_MAX;
    float    Radius = 0.0f;
};

static hz::CVar GGTAORadius("r.GTAO.Radius", 1.0f, 0.0001f, "GTAO search radius in meters.");

GTAO::GTAO(hz::RenderContext& Context)
{
    HorizonPipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "GTAO.slang" },
          .Name = "Renderer.GTAO.HorizonSearch" });
    UpsamplePipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "BlurMain" } }, .pFileName = "GTAO.slang" },
          .Name = "Renderer.GTAO.Upsample" });
    ASSERT(HorizonPipeline.IsValid() && UpsamplePipeline.IsValid());
}

void GTAO::Execute(hz::CommandList& Commands, hz::BufferSrv Frame, const hz::GPUTexture& Depth, const hz::GPUTexture& Normals,
                   const hz::GPUTexture& Occlusion, const hz::GPUTexture& Scratch)
{
    hz::DependencyList  HorizonDependencies;
    const GTAOConstants Horizon = { .FrameIndex = HorizonDependencies.Read(Frame),
                                    .DepthIndex = HorizonDependencies.Sample(&Depth),
                                    .NormalIndex = HorizonDependencies.Sample(&Normals),
                                    .OutputIndex = HorizonDependencies.Store(&Scratch),
                                    .Radius = GGTAORadius.GetFloat() };
    Commands.BeginGpuTimestamp("GTAO");
    Commands.SetPipeline(HorizonPipeline);
    Commands.SetRootConstants(Horizon);
    Commands.Dispatch((Scratch.GetWidth() + 15) / 16, (Scratch.GetHeight() + 15) / 16, 1, HorizonDependencies.Get());

    hz::DependencyList  UpsampleDependencies;
    const GTAOConstants Upsample = { .DepthIndex = UpsampleDependencies.Sample(&Depth),
                                     .NormalIndex = UpsampleDependencies.Sample(&Normals),
                                     .SourceIndex = UpsampleDependencies.Sample(&Scratch),
                                     .OutputIndex = UpsampleDependencies.Store(&Occlusion) };
    Commands.SetPipeline(UpsamplePipeline);
    Commands.SetRootConstants(Upsample);
    Commands.Dispatch((Occlusion.GetWidth() + 15) / 16, (Occlusion.GetHeight() + 15) / 16, 1, UpsampleDependencies.Get());
    Commands.EndGpuTimestamp();
}
