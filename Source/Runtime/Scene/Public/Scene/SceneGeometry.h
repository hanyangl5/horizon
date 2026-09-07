/* Copyright (c) 2026 Horizon */
#pragma once

#include "Graphics/RenderContext.h"

// Owned by SceneManager. References remain valid until the scene is released.
struct SceneGeometry
{
    hz::GPUBuffer                     mIndexBuffer;
    hz::GPUBuffer                     mVertexBuffers[MAX_VERTEX_BINDINGS];
    const IndirectDrawIndexArguments* pDrawArgs = nullptr;
    uint32_t                          mVertexStrides[MAX_VERTEX_BINDINGS] = {};
    uint32_t                          mVertexBufferCount = 0;
    IndexType                         mIndexType = INDEX_TYPE_UINT16;
    uint32_t                          mDrawArgCount = 0;
    uint32_t                          mIndexCount = 0;
    uint32_t                          mVertexCount = 0;
};
