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
    ranges.clear();
    if (indices.empty())
        return;
    qsort(indices.data(), indices.size(), sizeof(uint32_t), compareRenderIndices);
    for (uint32_t index : indices)
    {
        if (!ranges.empty() && index <= ranges[ranges.size() - 1].first + ranges[ranges.size() - 1].count)
        {
            RenderRange& range = ranges[ranges.size() - 1];
            range.count = index - range.first + 1;
        }
        else
            ranges.pushBack({ .first = index, .count = 1 });
    }
}

template<typename T, typename ID>
class RenderSlots
{
public:
    void reserve(uint32_t count)
    {
        values.reserve(count);
        active.reserve(count);
        positions.reserve(count);
        free.reserve(count);
        dirty.reserve(count);
        dirtyFlags.reserve(count);
        pending.reserve(count);
        pendingFlags.reserve(count);
        created.reserve(count);
        removed.reserve(count);
        ranges.reserve(count);
    }
    void beginUpdate()
    {
        for (uint32_t index : dirty)
            dirtyFlags[index] = false;
        dirty.clear();
        created.clear();
        removed.clear();
        ranges.clear();
    }
    ID add()
    {
        uint32_t index = values.size();
        if (!free.empty())
        {
            index = free[free.size() - 1];
            free.popBack();
        }
        else
        {
            reserve(index + 1);
            values.pushBack({});
            positions.pushBack(0);
            dirtyFlags.pushBack(false);
            pendingFlags.pushBack(false);
        }
        T& value = values[index];
        value.id = { .index = index, .generation = value.id.generation ? value.id.generation : 1 };
        positions[index] = active.size();
        active.pushBack(index);
        created.pushBack(index);
        pendingActive = true;
        markDirty(index);
        return value.id;
    }
    void remove(ID id)
    {
        ASSERT(get(id));
        const uint32_t position = positions[id.index];
        const uint32_t last = active[active.size() - 1];
        active[position] = last;
        positions[last] = position;
        active.popBack();
        const uint32_t generation = id.generation + 1;
        values[id.index] = {};
        values[id.index].id.generation = generation ? generation : 1;
        free.pushBack(id.index);
        removed.pushBack(id.index);
        pendingActive = true;
        markDirty(id.index);
    }
    const T* get(ID id) const
    {
        return id.isValid() && id.index < values.size() && values[id.index].id == id ? &values[id.index] : nullptr;
    }
    void markDirty(uint32_t index)
    {
        if (!dirtyFlags[index])
        {
            dirtyFlags[index] = true;
            dirty.pushBack(index);
        }
        if (!pendingFlags[index])
        {
            pendingFlags[index] = true;
            pending.pushBack(index);
        }
    }
    // Slots changed since the last clearPending(), accumulated across updates for GPU upload.
    void getPending(Array<RenderRange>& output) { mergeRenderRanges(pending, output); }
    bool isActivePending() const { return pendingActive; }
    void clearPending()
    {
        for (uint32_t index : pending)
        {
            pendingFlags[index] = false;
        }
        pending.clear();
        pendingActive = false;
    }
    void              finishUpdate() { mergeRenderRanges(dirty, ranges); }
    RenderSlotChanges getChanges() const
    {
        return { .created = { created.data(), created.size() },
                 .removed = { removed.data(), removed.size() },
                 .ranges = { ranges.data(), ranges.size() },
                 .activeList = !created.empty() || !removed.empty() };
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
