/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Core/IContainer.h"
#include <stdlib.h>

namespace hz
{
inline int CompareRenderIndices(const void* Left, const void* Right)
{
    const uint32_t LeftIndex = *(const uint32_t*)Left, RightIndex = *(const uint32_t*)Right;
    return (LeftIndex > RightIndex) - (LeftIndex < RightIndex);
}
inline void MergeRenderRanges(Array<uint32_t>& Indices, Array<RenderRange>& Ranges)
{
    Ranges.Clear();
    if (Indices.Empty())
    {
        return;
    }
    qsort(Indices.GetData(), Indices.Size(), sizeof(uint32_t), CompareRenderIndices);
    for (uint32_t Index : Indices)
    {
        if (!Ranges.Empty() && Index <= Ranges[Ranges.Size() - 1].first + Ranges[Ranges.Size() - 1].Count)
        {
            RenderRange& Range = Ranges[Ranges.Size() - 1];
            Range.Count = Index - Range.first + 1;
        }
        else
        {
            Ranges.PushBack({ .first = Index, .Count = 1 });
        }
    }
}

template<typename T, typename ID>
class RenderStorage
{
public:
    void Reserve(uint32_t Count)
    {
        Values.Reserve(Count);
        Active.Reserve(Count);
        Positions.Reserve(Count);
        Free.Reserve(Count);
        Dirty.Reserve(Count);
        DirtyFlags.Reserve(Count);
        Pending.Reserve(Count);
        PendingFlags.Reserve(Count);
        Created.Reserve(Count);
        Removed.Reserve(Count);
        Ranges.Reserve(Count);
    }
    void BeginUpdate()
    {
        for (uint32_t Index : Dirty)
        {
            DirtyFlags[Index] = false;
        }
        Dirty.Clear();
        Created.Clear();
        Removed.Clear();
        Ranges.Clear();
    }
    ID Add()
    {
        uint32_t Index = Values.Size();
        if (!Free.Empty())
        {
            Index = Free[Free.Size() - 1];
            Free.PopBack();
        }
        else
        {
            Reserve(Index + 1);
            Values.PushBack({});
            Positions.PushBack(0);
            DirtyFlags.PushBack(false);
            PendingFlags.PushBack(false);
        }
        T& Value = Values[Index];
        Value.id = { .index = Index, .generation = Value.id.generation ? Value.id.generation : 1 };
        Positions[Index] = Active.Size();
        Active.PushBack(Index);
        Created.PushBack(Index);
        PendingActive = true;
        MarkDirty(Index);
        return Value.id;
    }
    void Remove(ID Id)
    {
        ASSERT(Get(Id));
        const uint32_t Position = Positions[Id.index];
        const uint32_t Last = Active[Active.Size() - 1];
        Active[Position] = Last;
        Positions[Last] = Position;
        Active.PopBack();
        const uint32_t Generation = Id.generation + 1;
        Values[Id.index] = {};
        Values[Id.index].id.generation = Generation ? Generation : 1;
        Free.PushBack(Id.index);
        Removed.PushBack(Id.index);
        PendingActive = true;
        MarkDirty(Id.index);
    }
    const T* Get(ID Id) const
    {
        return Id.IsValid() && Id.index < Values.Size() && Values[Id.index].id == Id ? &Values[Id.index] : nullptr;
    }
    void MarkDirty(uint32_t Index)
    {
        if (!DirtyFlags[Index])
        {
            DirtyFlags[Index] = true;
            Dirty.PushBack(Index);
        }
        if (!PendingFlags[Index])
        {
            PendingFlags[Index] = true;
            Pending.PushBack(Index);
        }
    }
    // Entries changed since the last clearPending(), accumulated across updates for GPU Upload.
    void GetPending(Array<RenderRange>& Output) { MergeRenderRanges(Pending, Output); }
    bool IsActivePending() const { return PendingActive; }
    void ClearPending()
    {
        for (uint32_t Index : Pending)
        {
            PendingFlags[Index] = false;
        }
        Pending.Clear();
        PendingActive = false;
    }
    void          FinishUpdate() { MergeRenderRanges(Dirty, Ranges); }
    RenderChanges GetChanges() const
    {
        return { .created = { Created.GetData(), Created.Size() },
                 .removed = { Removed.GetData(), Removed.Size() },
                 .ranges = { Ranges.GetData(), Ranges.Size() },
                 .activeList = !Created.Empty() || !Removed.Empty() };
    }

    Array<T>        Values;
    Array<uint32_t> Active;

private:
    Array<uint32_t>    Positions;
    Array<uint32_t>    Free;
    Array<uint32_t>    Dirty;
    Array<bool>        DirtyFlags;
    Array<uint32_t>    Pending;
    Array<bool>        PendingFlags;
    Array<uint32_t>    Created;
    Array<uint32_t>    Removed;
    Array<RenderRange> Ranges;
    bool               PendingActive = false;
};
} // namespace hz
