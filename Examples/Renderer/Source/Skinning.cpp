#include "Skinning.h"
#include "Profiler/IProfiler.h"
#include "Scene/GpuSkinning.h"

// Mirrors the RootConstant0 block of Skinning.slang field for field.
struct SkinningConstants
{
    uint32_t jobsIndex;
    uint32_t paletteIndex;
};

SkinningPass::SkinningPass(hz::RenderContext& Context)
{
    Pipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "Skinning.slang" },
        .Name = "Renderer.Skinning",
    });
    ASSERT(Pipeline.IsValid());
}

void SkinningPass::Execute(hz::CommandList& Commands, const hz::GpuSkinning& Skinning, uint32_t FrameIndex)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("SkinningPass::Execute");
    if (!Skinning.GetJobCount())
    {
        return;
    }
    const SkinningConstants Constants = { .jobsIndex = Skinning.GetJobs(FrameIndex).GetSrvIndex(),
                                          .paletteIndex = Skinning.GetPalette(FrameIndex).GetSrvIndex() };
    Commands.BeginGpuTimestamp("Skinning");
    Commands.SetPipeline(Pipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Skinning.GetMaxVertexCount() + 63) / 64, Skinning.GetJobCount(), 1, Skinning.GetDependencies(FrameIndex));
    Commands.EndGpuTimestamp();
}
