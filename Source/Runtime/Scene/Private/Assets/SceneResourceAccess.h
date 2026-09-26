/* Copyright (c) 2026 Horizon */
#pragma once

#include "Graphics/RenderContext.h"

namespace hz
{
// Adopt ResourceLoader outputs after their upload token has completed.
struct SceneResourceAccess
{
    static GPUBuffer take(RenderContext* pContext, Buffer*& pBuffer, ResourceState state)
    {
        GPUBuffer result(pContext, pBuffer, pBuffer->size, (ResourceMemoryUsage)pBuffer->memoryUsage, (DescriptorType)pBuffer->descriptors,
                         state);
        pBuffer = nullptr;
        return result;
    }

    static GPUTexture take(RenderContext* pContext, Texture*& pTexture)
    {
        GPUTexture result(pContext, pTexture, nullptr, RESOURCE_STATE_SHADER_RESOURCE);
        pTexture = nullptr;
        return result;
    }
};
} // namespace hz
