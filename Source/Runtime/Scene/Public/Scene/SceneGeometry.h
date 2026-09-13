/* Copyright (c) 2026 Horizon */
#pragma once

#include "Graphics/RenderContext.h"

// Owned by SceneManager. References remain valid until the scene is released.
struct SceneGeometry
{
    hz::GPUBuffer                     indexBuffer;
    hz::GPUBuffer                     vertexBuffers[MAX_VERTEX_BINDINGS];
    const IndirectDrawIndexArguments* pDrawArgs = nullptr;
    uint32_t                          vertexStrides[MAX_VERTEX_BINDINGS] = {};
    uint32_t                          vertexBufferCount = 0;
    IndexType                         indexType = INDEX_TYPE_UINT16;
    uint32_t                          drawArgCount = 0;
    uint32_t                          indexCount = 0;
    uint32_t                          vertexCount = 0;
};
