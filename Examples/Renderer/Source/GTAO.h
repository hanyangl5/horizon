#pragma once

#include "Graphics/RenderContext.h"

// Half-resolution GTAO followed by a bilateral upsample to the lighting resolution.
class GTAO
{
public:
    static constexpr hz::Format TargetFormat = hz::Format::R8G8B8A8_UNORM;

    explicit GTAO(hz::RenderContext& Context);
    void Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const hz::GPUTexture& Depth, const hz::GPUTexture& Normals,
                 const hz::GPUTexture& Occlusion, const hz::GPUTexture& Scratch);

private:
    hz::GPUPipeline HorizonPipeline;
    hz::GPUPipeline UpsamplePipeline;
};
