/* Copyright (c) 2026 Horizon */

#include "FrameGraphInternal.h"
#include "../RenderContext/TextureDescs.h"

#include "Core/ILog.h"
#include "Profiler/IProfiler.h"

#include "Core/IMemory.h"

namespace hz
{
FrameGraph* InitFrameGraph(RenderContext* Context)
{
    FrameGraph* Graph = MemNew(FrameGraph);
    Graph->pContext = Context;
    return Graph;
}

void ExitFrameGraph(FrameGraph* Graph) { MemDelete(Graph); }

FrameGraphTexture AddFrameGraphTexture(FrameGraph* Graph, const FrameGraphTextureDesc& Desc)
{
    ASSERT(Graph && Graph->Textures.Size() < UINT16_MAX);
    FrameGraphTextureEntry& Entry = Graph->Textures.EmplaceBack();
    Entry.Desc = Desc;
    Entry.Kind = Desc.History ? FrameGraphResourceKind::History : FrameGraphResourceKind::Transient;
    Entry.Name = Desc.Texture.Name;
    Graph->Dirty = true;
    return { (uint16_t)(Graph->Textures.Size() - 1) };
}

FrameGraphBuffer AddFrameGraphBuffer(FrameGraph* Graph, const FrameGraphBufferDesc& Desc)
{
    ASSERT(Graph && Graph->Buffers.Size() < UINT16_MAX);
    FrameGraphBufferEntry& Entry = Graph->Buffers.EmplaceBack();
    Entry.Desc = Desc;
    Entry.Name = Desc.Buffer.Name;
    Graph->Dirty = true;
    return { (uint16_t)(Graph->Buffers.Size() - 1) };
}

FrameGraphTexture ImportFrameGraphTexture(FrameGraph* Graph, const FrameGraphImportDesc& Desc)
{
    ASSERT(Graph && Graph->Textures.Size() < UINT16_MAX);
    FrameGraphTextureEntry& Entry = Graph->Textures.EmplaceBack();
    Entry.Kind = FrameGraphResourceKind::Imported;
    Entry.Output = Desc.Output;
    Entry.Name = Desc.Name;
    Graph->Dirty = true;
    return { (uint16_t)(Graph->Textures.Size() - 1) };
}

FrameGraphBuffer ImportFrameGraphBuffer(FrameGraph* Graph, const FrameGraphImportDesc& Desc)
{
    ASSERT(Graph && Graph->Buffers.Size() < UINT16_MAX);
    FrameGraphBufferEntry& Entry = Graph->Buffers.EmplaceBack();
    Entry.Kind = FrameGraphResourceKind::Imported;
    Entry.Output = Desc.Output;
    Entry.Name = Desc.Name;
    Graph->Dirty = true;
    return { (uint16_t)(Graph->Buffers.Size() - 1) };
}

FrameGraphPass AddFrameGraphPass(FrameGraph* Graph, const FrameGraphPassDesc& Desc)
{
    ASSERT(Graph && Desc.Execute && Graph->Passes.Size() < UINT16_MAX);
    FrameGraphPassEntry& Entry = Graph->Passes.EmplaceBack();
    Entry.Desc = Desc;
    Entry.Desc.Textures = {};
    Entry.Desc.Buffers = {};
    Entry.FirstTextureUse = Graph->TextureUses.Size();
    Entry.TextureUseCount = Desc.Textures.Count;
    Entry.FirstBufferUse = Graph->BufferUses.Size();
    Entry.BufferUseCount = Desc.Buffers.Count;
    for (const FrameGraphTextureUse& Use : Desc.Textures)
    {
        ASSERT(Use.Texture.Index < Graph->Textures.Size());
        Graph->TextureUses.PushBack(Use);
    }
    for (const FrameGraphBufferUse& Use : Desc.Buffers)
    {
        ASSERT(Use.Buffer.Index < Graph->Buffers.Size());
        Graph->BufferUses.PushBack(Use);
    }
    Graph->Dirty = true;
    return { (uint16_t)(Graph->Passes.Size() - 1) };
}

void SetFrameGraphTextureImport(FrameGraph* Graph, FrameGraphTexture Texture, const GPUTexture* pTexture)
{
    ASSERT(Graph->Textures[Texture.Index].Kind == FrameGraphResourceKind::Imported);
    Graph->Textures[Texture.Index].pImported = pTexture;
}

void SetFrameGraphBufferImport(FrameGraph* Graph, FrameGraphBuffer Buffer, const GPUBuffer* pBuffer)
{
    ASSERT(Graph->Buffers[Buffer.Index].Kind == FrameGraphResourceKind::Imported);
    Graph->Buffers[Buffer.Index].pImported = pBuffer;
}

void SetFrameGraphPassEnabled(FrameGraph* Graph, FrameGraphPass Pass, bool Enabled)
{
    FrameGraphPassEntry& Entry = Graph->Passes[Pass.Index];
    if (Entry.Desc.Enabled == Enabled)
    {
        return;
    }
    Entry.Desc.Enabled = Enabled;
    Graph->Dirty = true;
}

void InvalidateFrameGraphHistory(FrameGraph* Graph, FrameGraphTexture Texture)
{
    ASSERT(Graph->Textures[Texture.Index].Kind == FrameGraphResourceKind::History);
    Graph->Textures[Texture.Index].HistoryValid = false;
}

bool ResizeFrameGraph(FrameGraph* Graph, uint32_t Width, uint32_t Height)
{
    if (!Width || !Height)
    {
        return false;
    }
    if (Graph->Width == Width && Graph->Height == Height)
    {
        return true;
    }
    Graph->Width = Width;
    Graph->Height = Height;
    for (FrameGraphTextureEntry& Entry : Graph->Textures)
    {
        Entry.HistoryValid = false;
    }
    Graph->Dirty = true;
    return CompileFrameGraph(Graph);
}

bool IsFrameGraphPassLive(const FrameGraph* Graph, FrameGraphPass Pass) { return Graph->Compiled && Graph->Passes[Pass.Index].Live; }

void ExecuteFrameGraph(FrameGraph* Graph, CommandList& Commands, void* FrameData)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ExecuteFrameGraph");
    if (Graph->Dirty && !CompileFrameGraph(Graph))
    {
        return;
    }
    for (uint32_t Index = 0; Index < Graph->Passes.Size(); ++Index)
    {
        const FrameGraphPassEntry& Pass = Graph->Passes[Index];
        if (!Pass.Live)
        {
            continue;
        }
        const FrameGraphContext Context = {
            .Commands = Commands,
            .Graph = *Graph,
            .Pass = { (uint16_t)Index },
            .FrameData = FrameData,
            .Width = Graph->Width,
            .Height = Graph->Height,
        };
        Pass.Desc.Execute(Context, Pass.Desc.UserData);
    }
    FinishFrameGraphFrame(Graph);
}

void FinishFrameGraphFrame(FrameGraph* Graph)
{
    for (FrameGraphTextureEntry& Entry : Graph->Textures)
    {
        if (Entry.Kind != FrameGraphResourceKind::History)
        {
            continue;
        }
        // Last frame's copy is valid only if a live pass wrote this frame's copy.
        Entry.HistoryValid = Entry.Produced;
        Entry.HistoryCurrent ^= 1u;
    }
}

#if defined(HZ_DEBUG) || defined(_DEBUG)
static bool passDeclares(const FrameGraph& Graph, FrameGraphPass Pass, FrameGraphTexture Texture, bool History)
{
    const FrameGraphPassEntry& Entry = Graph.Passes[Pass.Index];
    for (uint32_t i = 0; i < Entry.TextureUseCount; ++i)
    {
        const FrameGraphTextureUse& Use = Graph.TextureUses[Entry.FirstTextureUse + i];
        if (Use.Texture.Index == Texture.Index && (Use.Access == FrameGraphAccessReadHistory) == History)
        {
            return true;
        }
    }
    return false;
}
#endif

const GPUTexture* GetFrameGraphTexture(const FrameGraphContext& Context, FrameGraphTexture Texture)
{
    const FrameGraph&             Graph = Context.Graph;
    const FrameGraphTextureEntry& Entry = Graph.Textures[Texture.Index];
#if defined(HZ_DEBUG) || defined(_DEBUG)
    ASSERTMSG(passDeclares(Graph, Context.Pass, Texture, false), "Pass '%s' reads texture '%s' it did not declare",
              Graph.Passes[Context.Pass.Index].Desc.Name, Entry.Name ? Entry.Name : "");
#endif
    switch (Entry.Kind)
    {
    case FrameGraphResourceKind::Imported:
        return Entry.pImported;
    case FrameGraphResourceKind::History:
        return &Entry.HistoryTextures[Entry.HistoryCurrent];
    default:
        return Entry.Physical == UINT32_MAX ? nullptr : &Graph.Physical[Entry.Physical].Texture;
    }
}

const GPUTexture* GetFrameGraphHistory(const FrameGraphContext& Context, FrameGraphTexture Texture)
{
    const FrameGraphTextureEntry& Entry = Context.Graph.Textures[Texture.Index];
#if defined(HZ_DEBUG) || defined(_DEBUG)
    ASSERT(passDeclares(Context.Graph, Context.Pass, Texture, true));
#endif
    ASSERT(Entry.Kind == FrameGraphResourceKind::History);
    return &Entry.HistoryTextures[Entry.HistoryCurrent ^ 1u];
}

bool IsFrameGraphHistoryValid(const FrameGraphContext& Context, FrameGraphTexture Texture)
{
    return Context.Graph.Textures[Texture.Index].HistoryValid;
}

const GPUBuffer* GetFrameGraphBuffer(const FrameGraphContext& Context, FrameGraphBuffer Buffer)
{
    const FrameGraphBufferEntry& Entry = Context.Graph.Buffers[Buffer.Index];
    if (Entry.Kind == FrameGraphResourceKind::Imported)
    {
        return Entry.pImported;
    }
    return Entry.Produced ? &Entry.Buffer : nullptr;
}
} // namespace hz
