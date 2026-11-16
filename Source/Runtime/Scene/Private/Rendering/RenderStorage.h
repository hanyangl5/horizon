/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/RenderScene.h"
#include "Core/IContainer.h"
#include <stdlib.h>

namespace hz
{
inline int compareRenderIndices(const void* pA, const void* pB)
{
    const uint32_t a = *(const uint32_t*)pA, b = *(const uint32_t*)pB;
    return (a > b) - (a < b);
}
inline void mergeRenderRanges(Array<uint32_t>& indices, Array<RenderRange>& ranges)
{
    ranges.Clear();
    if (indices.Empty())
    {
        return;
    }
    qsort(indices.GetData(), indices.Size(), sizeof(uint32_t), compareRenderIndices);
    for (uint32_t index : indices)
    {
        if (!ranges.Empty() && index <= ranges[ranges.Size() - 1].first + ranges[ranges.Size() - 1].Count)
        {
            RenderRange& range = ranges[ranges.Size() - 1];
            range.Count = index - range.first + 1;
        }
        else
        {
            ranges.PushBack({ .first = index, .Count = 1 });
        }
    }
}

template<typename T, typename ID>
class RenderStorage
{
public:
    void Reserve(uint32_t Count)
    {
        values.Reserve(Count);
        active.Reserve(Count);
        positions.Reserve(Count);
        free.Reserve(Count);
        dirty.Reserve(Count);
        dirtyFlags.Reserve(Count);
        pending.Reserve(Count);
        pendingFlags.Reserve(Count);
        created.Reserve(Count);
        removed.Reserve(Count);
        ranges.Reserve(Count);
    }
    void beginUpdate()
    {
        for (uint32_t index : dirty)
        {
            dirtyFlags[index] = false;
        }
        dirty.Clear();
        created.Clear();
        removed.Clear();
        ranges.Clear();
    }
    ID Add()
    {
        uint32_t index = values.Size();
        if (!free.Empty())
        {
            index = free[free.Size() - 1];
            free.PopBack();
        }
        else
        {
            Reserve(index + 1);
            values.PushBack({});
            positions.PushBack(0);
            dirtyFlags.PushBack(false);
            pendingFlags.PushBack(false);
        }
        T& value = values[index];
        value.id = { .index = index, .generation = value.id.generation ? value.id.generation : 1 };
        positions[index] = active.Size();
        active.PushBack(index);
        created.PushBack(index);
        pendingActive = true;
        markDirty(index);
        return value.id;
    }
    void Remove(ID id)
    {
        ASSERT(Get(id));
        const uint32_t position = positions[id.index];
        const uint32_t last = active[active.Size() - 1];
        active[position] = last;
        positions[last] = position;
        active.PopBack();
        const uint32_t generation = id.generation + 1;
        values[id.index] = {};
        values[id.index].id.generation = generation ? generation : 1;
        free.PushBack(id.index);
        removed.PushBack(id.index);
        pendingActive = true;
        markDirty(id.index);
    }
    const T* Get(ID id) const
    {
        return id.IsValid() && id.index < values.Size() && values[id.index].id == id ? &values[id.index] : nullptr;
    }
    void markDirty(uint32_t index)
    {
        if (!dirtyFlags[index])
        {
            dirtyFlags[index] = true;
            dirty.PushBack(index);
        }
        if (!pendingFlags[index])
        {
            pendingFlags[index] = true;
            pending.PushBack(index);
        }
    }
    // Entries changed since the last clearPending(), accumulated across updates for GPU upload.
    void getPending(Array<RenderRange>& output) { mergeRenderRanges(pending, output); }
    bool isActivePending() const { return pendingActive; }
    void clearPending()
    {
        for (uint32_t index : pending)
        {
            pendingFlags[index] = false;
        }
        pending.Clear();
        pendingActive = false;
    }
    void          finishUpdate() { mergeRenderRanges(dirty, ranges); }
    RenderChanges getChanges() const
    {
        return { .created = { created.GetData(), created.Size() },
                 .removed = { removed.GetData(), removed.Size() },
                 .ranges = { ranges.GetData(), ranges.Size() },
                 .activeList = !created.Empty() || !removed.Empty() };
    }

    Array<T>        values;
    Array<uint32_t> active;

private:
    Array<uint32_t>    positions;
    Array<uint32_t>    free;
    Array<uint32_t>    dirty;
    Array<bool>        dirtyFlags;
    Array<uint32_t>    pending;
    Array<bool>        pendingFlags;
    Array<uint32_t>    created;
    Array<uint32_t>    removed;
    Array<RenderRange> ranges;
    bool               pendingActive = false;
};
} // namespace hz
