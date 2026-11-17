/* Copyright (c) 2026 Horizon */
#pragma once

#include "Graphics/RenderContext.h"

namespace hz
{
// Two textures with equal descs are interchangeable; the name is a debug label only.
inline bool SameTextureDesc(const TextureDesc& a, const TextureDesc& b)
{
    return a.Width == b.Width && a.Height == b.Height && a.Depth == b.Depth && a.ArraySize == b.ArraySize &&
           a.MipLevels == b.MipLevels && a.SampleCount == b.SampleCount && a.Format == b.Format && a.StartState == b.StartState &&
           a.Descriptors == b.Descriptors && a.Flags == b.Flags && a.GetRenderTarget == b.GetRenderTarget;
}
} // namespace hz
