/* Copyright (c) 2026 Horizon */
#pragma once

#include "Graphics/RenderContext.h"

namespace hz
{
// A frame graph is a list of passes registered once at initialization. Each pass declares the textures and buffers it
// reads and writes; compiling the graph culls passes whose results nothing needs, resolves viewport-relative sizes,
// and lets transient textures with disjoint lifetimes share one GPU texture. Barriers stay with the command list, which
// transitions every resource a command declares in its Dependencies, so the graph never tracks resource states.
struct FrameGraph;

struct FrameGraphTexture
{
    uint16_t Index = UINT16_MAX;
    bool     IsValid() const { return Index != UINT16_MAX; }
};
struct FrameGraphBuffer
{
    uint16_t Index = UINT16_MAX;
    bool     IsValid() const { return Index != UINT16_MAX; }
};
struct FrameGraphPass
{
    uint16_t Index = UINT16_MAX;
    bool     IsValid() const { return Index != UINT16_MAX; }
};

enum FrameGraphAccess : uint8_t
{
    // Needs a live pass that wrote the resource earlier this frame, or an imported resource.
    FrameGraphAccessRead,
    // Resolves to null when no live pass wrote the resource this frame.
    FrameGraphAccessReadOptional,
    // The copy of a history texture written last frame; never keeps a writer alive.
    FrameGraphAccessReadHistory,
    // Contents are undefined on entry; the pass must clear or overwrite them.
    FrameGraphAccessWrite,
    // Reads and writes, so the passes that wrote the resource earlier stay alive.
    FrameGraphAccessModify,
};

struct FrameGraphTextureUse
{
    FrameGraphTexture Texture;
    FrameGraphAccess  Access = FrameGraphAccessRead;
};
struct FrameGraphBufferUse
{
    FrameGraphBuffer Buffer;
    FrameGraphAccess Access = FrameGraphAccessRead;
};

struct FrameGraphTextureDesc
{
    // Width and Height 0 make the texture follow the viewport, shifted right by SizeShift and clamped to 1.
    TextureDesc Texture = {};
    uint32_t    SizeShift = 0;
    // Keeps two copies that survive across frames: the pass writes one and reads last frame's through ReadHistory.
    bool        History = false;
};
struct FrameGraphBufferDesc
{
    // A nonzero BytesPerPixel sizes the buffer to the viewport and recreates it on resize.
    BufferDesc Buffer = {};
    uint32_t   BytesPerPixel = 0;
};
struct FrameGraphImportDesc
{
    const char* Name = nullptr;
    // Marks a frame result, such as the backbuffer; passes that contribute to it are never culled.
    bool        Output = false;
};

struct FrameGraphContext
{
    CommandList&   Commands;
    FrameGraph&    Graph;
    FrameGraphPass Pass;
    void*          FrameData = nullptr;
    uint32_t       Width = 0;
    uint32_t       Height = 0;
};
typedef void (*FrameGraphExecuteFn)(const FrameGraphContext& Context, void* UserData);

struct FrameGraphPassDesc
{
    const char*                Name = nullptr;
    FrameGraphExecuteFn        Execute = nullptr;
    void*                      UserData = nullptr;
    // Copied when the pass is added.
    Span<FrameGraphTextureUse> Textures = {};
    Span<FrameGraphBufferUse>  Buffers = {};
    // Runs even when nothing reads its declared outputs, for example a pass that only writes CPU-visible statistics.
    bool                       SideEffects = false;
    bool                       Enabled = true;
};

// Context may be null to compile graphs without creating GPU objects, which the tests use.
FrameGraph* InitFrameGraph(RenderContext* Context);
void        ExitFrameGraph(FrameGraph* Graph);

FrameGraphTexture AddFrameGraphTexture(FrameGraph* Graph, const FrameGraphTextureDesc& Desc);
FrameGraphBuffer  AddFrameGraphBuffer(FrameGraph* Graph, const FrameGraphBufferDesc& Desc);
FrameGraphTexture ImportFrameGraphTexture(FrameGraph* Graph, const FrameGraphImportDesc& Desc);
FrameGraphBuffer  ImportFrameGraphBuffer(FrameGraph* Graph, const FrameGraphImportDesc& Desc);
FrameGraphPass    AddFrameGraphPass(FrameGraph* Graph, const FrameGraphPassDesc& Desc);

// Imported resources may change every frame, for example the swap chain image.
void SetFrameGraphTextureImport(FrameGraph* Graph, FrameGraphTexture Texture, const GPUTexture* pTexture);
void SetFrameGraphBufferImport(FrameGraph* Graph, FrameGraphBuffer Buffer, const GPUBuffer* pBuffer);
void SetFrameGraphPassEnabled(FrameGraph* Graph, FrameGraphPass Pass, bool Enabled);
void InvalidateFrameGraphHistory(FrameGraph* Graph, FrameGraphTexture Texture);

// Recreates viewport-sized resources and invalidates history. Replaced GPU objects are released once the GPU is done.
bool ResizeFrameGraph(FrameGraph* Graph, uint32_t Width, uint32_t Height);
// Runs automatically before execution after a pass was added, enabled or disabled, or the graph was resized.
bool CompileFrameGraph(FrameGraph* Graph);
bool IsFrameGraphPassLive(const FrameGraph* Graph, FrameGraphPass Pass);
// Records the live passes in order. Does not allocate once compiled.
void ExecuteFrameGraph(FrameGraph* Graph, CommandList& Commands, void* FrameData = nullptr);

// Inside a pass, these return the resources the pass declared; an optional read with no live writer returns null.
const GPUTexture* GetFrameGraphTexture(const FrameGraphContext& Context, FrameGraphTexture Texture);
const GPUTexture* GetFrameGraphHistory(const FrameGraphContext& Context, FrameGraphTexture Texture);
bool              IsFrameGraphHistoryValid(const FrameGraphContext& Context, FrameGraphTexture Texture);
const GPUBuffer*  GetFrameGraphBuffer(const FrameGraphContext& Context, FrameGraphBuffer Buffer);
} // namespace hz
