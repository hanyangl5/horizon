/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/AssetCatalog.h"
#include "Core/IContainer.h"
#include <stdlib.h>

namespace hz
{
inline int compareAssetIDs(const void* pA, const void* pB)
{
    const AssetID& a = *(const AssetID*)pA;
    const AssetID& b = *(const AssetID*)pB;
    return (b < a) - (a < b);
}

// Sorts ids[start, end) and drops invalid and repeated IDs, giving the order the catalog stores dependencies in.
inline Span<AssetID> normalizeAssetDependencies(Array<AssetID>& ids, uint32_t start = 0)
{
    if (start != ids.Size())
    {
        qsort(ids.GetData() + start, ids.Size() - start, sizeof(AssetID), compareAssetIDs);
    }
    uint32_t end = start;
    for (uint32_t i = start; i < ids.Size(); ++i)
    {
        if (ids[i].IsValid() && (end == start || ids[i] != ids[end - 1]))
        {
            ids[end++] = ids[i];
        }
    }
    ids.Resize(end);
    return { ids.GetData() + start, end - start };
}

// Loaded payloads must reference exactly the dependencies the catalog reserved for them.
inline bool matchesAssetDependencies(const AssetCatalogEntry& entry, Array<AssetID>& ids)
{
    const Span<AssetID> sorted = normalizeAssetDependencies(ids);
    if (sorted.Count != entry.dependencies.Count)
    {
        return false;
    }
    for (uint32_t i = 0; i < sorted.Count; ++i)
    {
        if (sorted.Data[i] != entry.dependencies.Data[i])
        {
            return false;
        }
    }
    return true;
}
} // namespace hz
