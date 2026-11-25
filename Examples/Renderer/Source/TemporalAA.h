#pragma once

#include "Graphics/RenderContext.h"

struct TemporalJitter
{
    float X = 0.0f;
    float Y = 0.0f;
};

struct TemporalAAInputs
{
    const hz::GPUTexture* Color = nullptr;
    const hz::GPUTexture* Opaque = nullptr;
    const hz::GPUTexture* Depth = nullptr;
    const hz::GPUTexture* Motion = nullptr;
    const hz::GPUTexture* History = nullptr;
};

class TemporalAA
{
public:
    explicit TemporalAA(hz::RenderContext& Context);
    static TemporalJitter GetJitter(uint32_t FrameIndex, uint32_t Width, uint32_t Height);
    void Execute(hz::CommandList& Commands, hz::BufferSrv Frame, const hz::GPUSampler& Sampler, const TemporalAAInputs& Inputs,
                 const hz::GPUTexture& Output, TemporalJitter Jitter, TemporalJitter PreviousJitter) const;

private:
    hz::GPUPipeline Pipeline;
};
