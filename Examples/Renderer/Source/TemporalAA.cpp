#include "TemporalAA.h"

struct TemporalAAConstants
{
    uint32_t CurrentIndex;
    uint32_t OpaqueIndex;
    uint32_t DepthIndex;
    uint32_t MotionIndex;
    uint32_t FrameIndex;
    uint32_t HistoryIndex;
    uint32_t OutputIndex;
    uint32_t SamplerIndex;
    float    JitterX;
    float    JitterY;
    float    PreviousJitterX;
    float    PreviousJitterY;
};

TemporalAA::TemporalAA(hz::RenderContext& Context)
{
    Pipeline = Context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } }, .pFileName = "TemporalAA.slang" },
          .Name = "Renderer.TemporalAA" });
    ASSERT(Pipeline.IsValid());
}

TemporalJitter TemporalAA::GetJitter(uint32_t FrameIndex, uint32_t Width, uint32_t Height)
{
    // Centered Halton(2, 3) samples in pixel units, expressed as UV offsets for the resolve.
    constexpr float JitterPixels[][2] = {
        { 0.0f, -1.0f / 6.0f },   { -0.25f, 1.0f / 6.0f },    { 0.25f, -7.0f / 18.0f }, { -0.375f, -1.0f / 18.0f },
        { 0.125f, 5.0f / 18.0f }, { -0.125f, -5.0f / 18.0f }, { 0.375f, 1.0f / 18.0f }, { -0.4375f, 7.0f / 18.0f },
    };
    const float* Sample = JitterPixels[FrameIndex % TF_ARRAY_COUNT(JitterPixels)];
    return { .X = Sample[0] / Width, .Y = Sample[1] / Height };
}

void TemporalAA::Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUSampler& Sampler,
                         const TemporalAAInputs& Inputs, const hz::GPUTexture& Output, TemporalJitter Jitter,
                         TemporalJitter PreviousJitter) const
{
    hz::DependencyList        Dependencies;
    const TemporalAAConstants Constants = {
        .CurrentIndex = Dependencies.Sample(Inputs.Color),
        .OpaqueIndex = Dependencies.Sample(Inputs.Opaque),
        .DepthIndex = Dependencies.Sample(Inputs.Depth),
        .MotionIndex = Dependencies.Sample(Inputs.Motion),
        .FrameIndex = Dependencies.Read(Frame),
        .HistoryIndex = Dependencies.Sample(Inputs.History),
        .OutputIndex = Dependencies.Store(&Output),
        .SamplerIndex = Sampler.GetIndex(),
        .JitterX = Jitter.X,
        .JitterY = Jitter.Y,
        .PreviousJitterX = PreviousJitter.X,
        .PreviousJitterY = PreviousJitter.Y,
    };
    Commands.BeginGpuTimestamp("Temporal AA");
    Commands.SetPipeline(Pipeline);
    Commands.SetRootConstants(Constants);
    Commands.Dispatch((Output.GetWidth() + 7) / 8, (Output.GetHeight() + 7) / 8, 1, Dependencies.Get());
    Commands.EndGpuTimestamp();
}
