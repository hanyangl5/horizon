#include "ScreenSpaceDenoise.h"
#include "RayTracedEffects.h"

// Mirrors the RootConstant0 block of the denoise shaders field for field.
struct SSDConstants
{
    uint32_t FrameIndex;
    uint32_t InputIndex;
    uint32_t HistoryIndex;
    uint32_t DepthIndex;
    uint32_t PreviousDepthIndex;
    uint32_t NormalIndex;
    uint32_t OutputIndex;
    uint32_t HistoryValid;
    float    TemporalAlpha;
    float    DepthThreshold;
    float    PlaneSigma;
    uint32_t StepWidth;
    uint32_t MotionIndex;
};
static_assert(sizeof(SSDConstants) == 13 * sizeof(uint32_t));

ScreenSpaceDenoise::ScreenSpaceDenoise(hz::RenderContext& Context)
{
    ShadowAOTemporalPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "ScreenSpaceDenoise/SSDShadowAOTemporalAccumulation.slang" },
        .Name = "Renderer.SSD.ShadowAO.Temporal",
    });
    ShadowAOSpatialPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "ScreenSpaceDenoise/SSDShadowAOSpatialAccumulation.slang" },
        .Name = "Renderer.SSD.ShadowAO.Spatial",
    });
    DepthHistoryPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSCopyDepth" } },
                        .pFileName = "ScreenSpaceDenoise/SSDDepthHistory.slang" },
        .Name = "Renderer.SSD.DepthHistory",
    });
    ReflectionTemporalPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "ScreenSpaceDenoise/SSDReflectionTemporalAccumulation.slang" },
        .Name = "Renderer.SSD.Reflection.Temporal",
    });
    ReflectionSpatialPipeline = Context.CreateComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "ScreenSpaceDenoise/SSDReflectionSpatialAccumulation.slang" },
        .Name = "Renderer.SSD.Reflection.Spatial",
    });
    ASSERT(ShadowAOTemporalPipeline.IsValid() && ShadowAOSpatialPipeline.IsValid() && DepthHistoryPipeline.IsValid() &&
           ReflectionTemporalPipeline.IsValid() && ReflectionSpatialPipeline.IsValid());
}

void ScreenSpaceDenoise::DenoiseShadowAO(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                                         const RayTracedEffectsSettings& Settings, const RayTracedEffectsTargets& Targets, bool UseHistory)
{
    const hz::GPUTexture& Result = *Targets.Result;
    const hz::GPUTexture& Scratch = *Targets.Scratch;
    const uint32_t        Width = Result.GetWidth();
    const uint32_t        Height = Result.GetHeight();
    Commands.BeginGpuTimestamp("SSD Shadow AO");
    const hz::GPUTexture& History = *Targets.History;
    const hz::GPUTexture& PreviousHistory = *Targets.PreviousHistory;
    const hz::GPUTexture& PreviousFrameDepth = *Targets.PreviousFrameDepth;
    SSDConstants          Denoise = {
                 .FrameIndex = Frame.GetSrvIndex(),
                 .InputIndex = Result.GetSrvIndex(),
                 .HistoryIndex = PreviousHistory.GetSrvIndex(),
                 .DepthIndex = GBuffer.Depth->GetSrvIndex(),
                 .PreviousDepthIndex = PreviousFrameDepth.GetSrvIndex(),
                 .NormalIndex = GBuffer.NormalMaterial->GetSrvIndex(),
                 .OutputIndex = History.GetUavIndex(),
                 .HistoryValid = Targets.HistoryValid && UseHistory,
                 .TemporalAlpha = Settings.TemporalAlpha,
                 .DepthThreshold = Settings.TemporalDepthThreshold,
                 .PlaneSigma = Settings.AtrousPlaneSigma,
                 .MotionIndex = GBuffer.Motion->GetSrvIndex(),
    };
    Commands.SetPipeline(ShadowAOTemporalPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &Result, &PreviousHistory, GBuffer.Depth, &PreviousFrameDepth, GBuffer.Motion },
                        .storageTextures = { &History },
                        .buffers = { &Frame } });
    // The traced input is consumed, so the a-trous iterations ping-pong through Scratch and finish in Result.
    const hz::GPUTexture* Passes[] = { &History, &Scratch, &Result };
    for (uint32_t Pass = 0; Pass < 2; ++Pass)
    {
        Denoise.InputIndex = Passes[Pass]->GetSrvIndex();
        Denoise.OutputIndex = Passes[Pass + 1]->GetUavIndex();
        Denoise.StepWidth = 1u << Pass;
        Commands.SetPipeline(ShadowAOSpatialPipeline);
        Commands.SetRootConstants(Denoise);
        Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                          { .sampledTextures = { Passes[Pass], GBuffer.Depth, GBuffer.NormalMaterial },
                            .storageTextures = { Passes[Pass + 1] },
                            .buffers = { &Frame } });
    }
    Commands.EndGpuTimestamp();
}

void ScreenSpaceDenoise::DenoiseReflections(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer,
                                            const RayTracedEffectsSettings& Settings, const RayTracedEffectsTargets& Targets,
                                            bool UseHistory)
{
    Commands.BeginGpuTimestamp("SSD Reflections");
    const hz::GPUTexture& Reflection = *Targets.Reflection;
    const hz::GPUTexture& PreviousFrameDepth = *Targets.PreviousFrameDepth;
    const uint32_t        Width = Reflection.GetWidth();
    const uint32_t        Height = Reflection.GetHeight();
    const hz::GPUTexture& ReflectionHistory = *Targets.ReflectionHistory;
    const hz::GPUTexture& PreviousReflectionHistory = *Targets.PreviousReflectionHistory;
    SSDConstants          Denoise = {
                 .FrameIndex = Frame.GetSrvIndex(),
                 .InputIndex = Reflection.GetSrvIndex(),
                 .HistoryIndex = PreviousReflectionHistory.GetSrvIndex(),
                 .DepthIndex = GBuffer.Depth->GetSrvIndex(),
                 .PreviousDepthIndex = PreviousFrameDepth.GetSrvIndex(),
                 .NormalIndex = GBuffer.NormalMaterial->GetSrvIndex(),
                 .OutputIndex = ReflectionHistory.GetUavIndex(),
                 .HistoryValid = Targets.ReflectionHistoryValid && Targets.HistoryValid && UseHistory,
                 .TemporalAlpha = Settings.ReflectionTemporalAlpha,
                 .DepthThreshold = Settings.TemporalDepthThreshold,
                 .PlaneSigma = Settings.AtrousPlaneSigma,
                 .MotionIndex = GBuffer.Motion->GetSrvIndex(),
    };
    Commands.SetPipeline(ReflectionTemporalPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &Reflection, &PreviousReflectionHistory, GBuffer.Depth, &PreviousFrameDepth, GBuffer.Motion },
                        .storageTextures = { &ReflectionHistory },
                        .buffers = { &Frame } });
    Denoise.InputIndex = ReflectionHistory.GetSrvIndex();
    Denoise.OutputIndex = Reflection.GetUavIndex();
    Commands.SetPipeline(ReflectionSpatialPipeline);
    Commands.SetRootConstants(Denoise);
    Commands.Dispatch((Width + 15) / 16, (Height + 15) / 16, 1,
                      { .sampledTextures = { &ReflectionHistory, GBuffer.Depth, GBuffer.NormalMaterial },
                        .storageTextures = { &Reflection },
                        .buffers = { &Frame } });
    Commands.EndGpuTimestamp();
}

void ScreenSpaceDenoise::StoreDepthHistory(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth,
                                           const hz::GPUTexture& Output)
{
    const SSDConstants Constants = { .FrameIndex = Frame.GetSrvIndex(),
                                     .DepthIndex = Depth.GetSrvIndex(),
                                     .OutputIndex = Output.GetUavIndex() };
    Commands.SetPipeline(DepthHistoryPipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Output.GetWidth() + 15) / 16, (Output.GetHeight() + 15) / 16, 1,
                      { .sampledTextures = { &Depth }, .storageTextures = { &Output }, .buffers = { &Frame } });
}
