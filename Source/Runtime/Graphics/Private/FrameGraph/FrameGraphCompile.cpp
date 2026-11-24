/* Copyright (c) 2026 Horizon */

#include "FrameGraphInternal.h"
#include "../RenderContext/TextureDescs.h"

#include "Core/ILog.h"
#include "Profiler/IProfiler.h"

#include "Core/IMemory.h"

namespace hz
{
static bool IsWrite(FrameGraphAccess Access) { return Access == FrameGraphAccessWrite || Access == FrameGraphAccessModify; }

static TextureDesc ResolveTexture(const FrameGraph& Graph, const FrameGraphTextureDesc& Desc)
{
    TextureDesc Resolved = Desc.Texture;
    if (!Resolved.Width || !Resolved.Height)
    {
        Resolved.Width = Graph.Width >> Desc.SizeShift;
        Resolved.Height = Graph.Height >> Desc.SizeShift;
        Resolved.Width = Resolved.Width ? Resolved.Width : 1;
        Resolved.Height = Resolved.Height ? Resolved.Height : 1;
    }
    return Resolved;
}

// Marks the live passes, walking backwards from the outputs. A Write fully replaces a resource, so it satisfies every later
// read and the passes that wrote it before become unneeded; Modify and the reads keep earlier writers alive.
static void CullPasses(FrameGraph& Graph)
{
    Graph.TextureNeeded.Resize(Graph.Textures.Size());
    Graph.BufferNeeded.Resize(Graph.Buffers.Size());
    for (uint32_t I = 0; I < Graph.Textures.Size(); ++I)
    {
        Graph.TextureNeeded[I] = Graph.Textures[I].Output;
    }
    for (uint32_t I = 0; I < Graph.Buffers.Size(); ++I)
    {
        Graph.BufferNeeded[I] = Graph.Buffers[I].Output;
    }
    for (uint32_t Index = Graph.Passes.Size(); Index > 0; --Index)
    {
        FrameGraphPassEntry& Pass = Graph.Passes[Index - 1];
        Pass.Live = false;
        if (!Pass.Desc.Enabled)
        {
            continue;
        }
        bool Live = Pass.Desc.SideEffects;
        for (uint32_t I = 0; I < Pass.TextureUseCount && !Live; ++I)
        {
            const FrameGraphTextureUse& Use = Graph.TextureUses[Pass.FirstTextureUse + I];
            Live = IsWrite(Use.Access) && Graph.Textures[Use.Texture.Index].Kind != FrameGraphResourceKind::History &&
                   Graph.TextureNeeded[Use.Texture.Index];
        }
        for (uint32_t I = 0; I < Pass.BufferUseCount && !Live; ++I)
        {
            const FrameGraphBufferUse& Use = Graph.BufferUses[Pass.FirstBufferUse + I];
            Live = IsWrite(Use.Access) && Graph.BufferNeeded[Use.Buffer.Index];
        }
        if (!Live)
        {
            continue;
        }
        Pass.Live = true;
        // Writes first, so a pass that writes and reads the same resource still needs an earlier writer.
        for (uint32_t I = 0; I < Pass.TextureUseCount; ++I)
        {
            const FrameGraphTextureUse& Use = Graph.TextureUses[Pass.FirstTextureUse + I];
            if (Use.Access == FrameGraphAccessWrite)
            {
                Graph.TextureNeeded[Use.Texture.Index] = false;
            }
        }
        for (uint32_t I = 0; I < Pass.BufferUseCount; ++I)
        {
            const FrameGraphBufferUse& Use = Graph.BufferUses[Pass.FirstBufferUse + I];
            if (Use.Access == FrameGraphAccessWrite)
            {
                Graph.BufferNeeded[Use.Buffer.Index] = false;
            }
        }
        for (uint32_t I = 0; I < Pass.TextureUseCount; ++I)
        {
            const FrameGraphTextureUse& Use = Graph.TextureUses[Pass.FirstTextureUse + I];
            if (Use.Access != FrameGraphAccessWrite && Use.Access != FrameGraphAccessReadHistory)
            {
                Graph.TextureNeeded[Use.Texture.Index] = true;
            }
        }
        for (uint32_t I = 0; I < Pass.BufferUseCount; ++I)
        {
            const FrameGraphBufferUse& Use = Graph.BufferUses[Pass.FirstBufferUse + I];
            if (Use.Access != FrameGraphAccessWrite && Use.Access != FrameGraphAccessReadHistory)
            {
                Graph.BufferNeeded[Use.Buffer.Index] = true;
            }
        }
    }
}

// Walks the live passes forward: every required read needs an earlier live writer or an import, and a transient texture's
// first access must write it, since a shared texture holds another resource's contents on entry.
static bool ValidateUses(FrameGraph& Graph)
{
    for (FrameGraphTextureEntry& Entry : Graph.Textures)
    {
        Entry.Produced = false;
    }
    for (FrameGraphBufferEntry& Entry : Graph.Buffers)
    {
        Entry.Produced = false;
    }
    for (uint32_t Index = 0; Index < Graph.Passes.Size(); ++Index)
    {
        const FrameGraphPassEntry& Pass = Graph.Passes[Index];
        if (!Pass.Live)
        {
            continue;
        }
        for (uint32_t I = 0; I < Pass.TextureUseCount; ++I)
        {
            const FrameGraphTextureUse&   Use = Graph.TextureUses[Pass.FirstTextureUse + I];
            const FrameGraphTextureEntry& Entry = Graph.Textures[Use.Texture.Index];
            const bool                    Available = Entry.Produced || Entry.Kind == FrameGraphResourceKind::Imported;
            if ((Use.Access == FrameGraphAccessRead || Use.Access == FrameGraphAccessModify) && !Available)
            {
                LOGF(Error, "Frame graph pass '%s' reads texture '%s' before any live pass writes it", Pass.Desc.Name,
                     Entry.Name ? Entry.Name : "");
                return false;
            }
            if (Use.Access == FrameGraphAccessReadHistory && Entry.Kind != FrameGraphResourceKind::History)
            {
                LOGF(Error, "Frame graph pass '%s' reads history of '%s', which is not a history texture", Pass.Desc.Name,
                     Entry.Name ? Entry.Name : "");
                return false;
            }
        }
        for (uint32_t I = 0; I < Pass.BufferUseCount; ++I)
        {
            const FrameGraphBufferUse&   Use = Graph.BufferUses[Pass.FirstBufferUse + I];
            const FrameGraphBufferEntry& Entry = Graph.Buffers[Use.Buffer.Index];
            if ((Use.Access == FrameGraphAccessRead || Use.Access == FrameGraphAccessModify) && !Entry.Produced &&
                Entry.Kind != FrameGraphResourceKind::Imported)
            {
                LOGF(Error, "Frame graph pass '%s' reads buffer '%s' before any live pass writes it", Pass.Desc.Name,
                     Entry.Name ? Entry.Name : "");
                return false;
            }
        }
        // Mark this pass's writes only after checking its reads, so reading its own output still needs an earlier writer.
        for (uint32_t I = 0; I < Pass.TextureUseCount; ++I)
        {
            const FrameGraphTextureUse& Use = Graph.TextureUses[Pass.FirstTextureUse + I];
            if (IsWrite(Use.Access))
            {
                Graph.Textures[Use.Texture.Index].Produced = true;
            }
        }
        for (uint32_t I = 0; I < Pass.BufferUseCount; ++I)
        {
            const FrameGraphBufferUse& Use = Graph.BufferUses[Pass.FirstBufferUse + I];
            if (IsWrite(Use.Access))
            {
                Graph.Buffers[Use.Buffer.Index].Produced = true;
            }
        }
    }
    return true;
}

// Gives every produced transient texture a physical texture. Textures share one when their descs match and the previous
// holder's last use comes strictly before the new holder's first use; the command list transitions the shared texture
// from whatever state the previous holder left it in, and the first access is always a write.
static void AssignPhysicalTextures(FrameGraph& Graph)
{
    const uint32_t Count = Graph.Textures.Size();
    Graph.FirstUse.Resize(Count);
    Graph.LastUse.Resize(Count);
    for (uint32_t I = 0; I < Count; ++I)
    {
        Graph.FirstUse[I] = UINT32_MAX;
        Graph.LastUse[I] = 0;
    }
    for (uint32_t Index = 0; Index < Graph.Passes.Size(); ++Index)
    {
        const FrameGraphPassEntry& Pass = Graph.Passes[Index];
        if (!Pass.Live)
        {
            continue;
        }
        for (uint32_t I = 0; I < Pass.TextureUseCount; ++I)
        {
            const uint32_t Texture = Graph.TextureUses[Pass.FirstTextureUse + I].Texture.Index;
            if (Graph.FirstUse[Texture] == UINT32_MAX)
            {
                Graph.FirstUse[Texture] = Index;
            }
            Graph.LastUse[Texture] = Index;
        }
    }
    for (FrameGraphPhysicalTexture& Physical : Graph.Physical)
    {
        Physical.Assigned = false;
    }
    for (FrameGraphTextureEntry& Entry : Graph.Textures)
    {
        Entry.Physical = UINT32_MAX;
    }
    for (uint32_t Index = 0; Index < Graph.Passes.Size(); ++Index)
    {
        if (!Graph.Passes[Index].Live)
        {
            continue;
        }
        for (uint32_t Texture = 0; Texture < Count; ++Texture)
        {
            FrameGraphTextureEntry& Entry = Graph.Textures[Texture];
            if (Entry.Kind != FrameGraphResourceKind::Transient || !Entry.Produced || Graph.FirstUse[Texture] != Index)
            {
                continue;
            }
            uint32_t Chosen = UINT32_MAX;
            for (uint32_t P = 0; P < Graph.Physical.Size() && Chosen == UINT32_MAX; ++P)
            {
                const FrameGraphPhysicalTexture& Physical = Graph.Physical[P];
                if (SameTextureDesc(Physical.Desc, Entry.Resolved) && (!Physical.Assigned || Physical.LastPass < Index))
                {
                    Chosen = P;
                }
            }
            if (Chosen == UINT32_MAX)
            {
                Chosen = Graph.Physical.Size();
                FrameGraphPhysicalTexture& Physical = Graph.Physical.EmplaceBack();
                Physical.Desc = Entry.Resolved;
            }
            FrameGraphPhysicalTexture& Physical = Graph.Physical[Chosen];
            Physical.Assigned = true;
            Physical.LastPass = Graph.LastUse[Texture];
            Entry.Physical = Chosen;
        }
    }
}

bool CompileFrameGraph(FrameGraph* Graph)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("CompileFrameGraph");
    Graph->Compiled = false;
    if (!Graph->Width || !Graph->Height)
    {
        return false;
    }
    for (FrameGraphTextureEntry& Entry : Graph->Textures)
    {
        if (Entry.Kind != FrameGraphResourceKind::Imported)
        {
            Entry.Resolved = ResolveTexture(*Graph, Entry.Desc);
        }
    }
    CullPasses(*Graph);
    if (!ValidateUses(*Graph))
    {
        return false;
    }
    AssignPhysicalTextures(*Graph);

    // Drop physical textures no texture uses any more, then create the missing GPU objects.
    uint32_t Kept = 0;
    for (uint32_t P = 0; P < Graph->Physical.Size(); ++P)
    {
        if (!Graph->Physical[P].Assigned)
        {
            continue;
        }
        if (Kept != P)
        {
            Graph->Physical[Kept] = std::move(Graph->Physical[P]);
            for (FrameGraphTextureEntry& Entry : Graph->Textures)
            {
                if (Entry.Physical == P)
                {
                    Entry.Physical = Kept;
                }
            }
        }
        ++Kept;
    }
    Graph->Physical.Resize(Kept);
    if (Graph->pContext)
    {
        for (FrameGraphPhysicalTexture& Physical : Graph->Physical)
        {
            if (!Physical.Texture.IsValid() || Physical.Texture.GetWidth() != Physical.Desc.Width ||
                Physical.Texture.GetHeight() != Physical.Desc.Height)
            {
                Physical.Texture = Graph->pContext->CreateTexture(Physical.Desc);
            }
        }
        for (FrameGraphTextureEntry& Entry : Graph->Textures)
        {
            if (Entry.Kind != FrameGraphResourceKind::History)
            {
                continue;
            }
            for (GPUTexture& Copy : Entry.HistoryTextures)
            {
                if (!Copy.IsValid() || Copy.GetWidth() != Entry.Resolved.Width || Copy.GetHeight() != Entry.Resolved.Height)
                {
                    Copy = Graph->pContext->CreateTexture(Entry.Resolved);
                    Entry.HistoryValid = false;
                }
            }
        }
        for (FrameGraphBufferEntry& Entry : Graph->Buffers)
        {
            if (Entry.Kind == FrameGraphResourceKind::Imported)
            {
                continue;
            }
            BufferDesc Desc = Entry.Desc.Buffer;
            if (Entry.Desc.BytesPerPixel)
            {
                Desc.Size = (uint64_t)Graph->Width * Graph->Height * Entry.Desc.BytesPerPixel;
                if (Desc.StructStride)
                {
                    Desc.ElementCount = (uint32_t)(Desc.Size / Desc.StructStride);
                }
            }
            if (!Entry.Buffer.IsValid() || Entry.ResolvedSize != Desc.Size)
            {
                Entry.Buffer = Graph->pContext->CreateBuffer(Desc);
                Entry.ResolvedSize = Desc.Size;
            }
        }
    }
    else
    {
        for (FrameGraphBufferEntry& Entry : Graph->Buffers)
        {
            if (Entry.Kind != FrameGraphResourceKind::Imported)
            {
                Entry.ResolvedSize =
                    Entry.Desc.BytesPerPixel ? (uint64_t)Graph->Width * Graph->Height * Entry.Desc.BytesPerPixel : Entry.Desc.Buffer.Size;
            }
        }
    }
    Graph->Dirty = false;
    Graph->Compiled = true;
    return true;
}
} // namespace hz
