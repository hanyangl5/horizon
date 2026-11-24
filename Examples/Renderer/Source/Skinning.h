#pragma once

#include "Graphics/RenderContext.h"

namespace hz
{
class GpuSkinning;
}

// Skins the frame's GpuSkinning jobs in one dispatch; the shader matches the renderer's vertex layout.
class SkinningPass
{
public:
    explicit SkinningPass(hz::RenderContext& Context);
    void Execute(hz::CommandList& Commands, const hz::GpuSkinning& Skinning, uint32_t FrameIndex);

private:
    hz::GPUPipeline Pipeline;
};
