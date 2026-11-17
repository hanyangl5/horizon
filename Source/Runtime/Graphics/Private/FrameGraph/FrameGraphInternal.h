/* Copyright (c) 2026 Horizon */
#pragma once

#include "Graphics/FrameGraph.h"
#include "Core/IContainer.h"

namespace hz
{
enum class FrameGraphResourceKind : uint8_t
{
    Transient,
    History,
    Imported,
};

struct FrameGraphTextureEntry
{
    FrameGraphTextureDesc  Desc;
    FrameGraphResourceKind Kind = FrameGraphResourceKind::Transient;
    bool                   Output = false;
    const char*            Name = nullptr;
    // Compiled state.
    TextureDesc            Resolved = {};
    uint32_t               Physical = UINT32_MAX;
    bool                   Produced = false;
    // History copies: Current is written this frame, the other holds last frame's contents.
    GPUTexture             HistoryTextures[2];
    uint32_t               HistoryCurrent = 0;
    bool                   HistoryValid = false;
    const GPUTexture*      pImported = nullptr;
};

struct FrameGraphBufferEntry
{
    FrameGraphBufferDesc   Desc;
    FrameGraphResourceKind Kind = FrameGraphResourceKind::Transient;
    bool                   Output = false;
    const char*            Name = nullptr;
    bool                   Produced = false;
    GPUBuffer              Buffer;
    uint64_t               ResolvedSize = 0;
    const GPUBuffer*       pImported = nullptr;
};

struct FrameGraphPassEntry
{
    FrameGraphPassDesc   Desc;
    // Uses are copied into the graph arrays; Desc's spans are not kept.
    uint32_t             FirstTextureUse = 0;
    uint32_t             TextureUseCount = 0;
    uint32_t             FirstBufferUse = 0;
    uint32_t             BufferUseCount = 0;
    bool                 Live = false;
};

// A GPU texture that one or more transient textures with disjoint lifetimes share.
struct FrameGraphPhysicalTexture
{
    TextureDesc Desc = {};
    GPUTexture  Texture;
    uint32_t    LastPass = 0;
    bool        Assigned = false;
};

struct FrameGraph
{
    RenderContext*                   pContext = nullptr;
    Array<FrameGraphTextureEntry>    Textures;
    Array<FrameGraphBufferEntry>     Buffers;
    Array<FrameGraphPassEntry>       Passes;
    Array<FrameGraphTextureUse>      TextureUses;
    Array<FrameGraphBufferUse>       BufferUses;
    Array<FrameGraphPhysicalTexture> Physical;
    // Compile scratch, kept to avoid reallocating.
    Array<bool>                      TextureNeeded;
    Array<bool>                      BufferNeeded;
    Array<uint32_t>                  FirstUse;
    Array<uint32_t>                  LastUse;
    uint32_t                         Width = 0;
    uint32_t                         Height = 0;
    bool                             Dirty = true;
    bool                             Compiled = false;
};

// Advances history after a frame: textures whose writer ran become valid and swap copies, the rest become invalid.
void FinishFrameGraphFrame(FrameGraph* Graph);
} // namespace hz
