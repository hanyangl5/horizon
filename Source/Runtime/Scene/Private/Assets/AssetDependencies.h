/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/AssetCatalog.h"
#include "Core/IContainer.h"
#include <stdlib.h>

namespace hz
{
inline int CompareAssetIDs(const void* Left, const void* Right)
{
    const AssetID& LeftId = *(const AssetID*)Left;
    const AssetID& RightId = *(const AssetID*)Right;
    return (RightId < LeftId) - (LeftId < RightId);
}

// Sorts ids[start, end) and drops invalid and repeated IDs, giving the order the catalog stores dependencies in.
inline Span<AssetID> NormalizeAssetDependencies(Array<AssetID>& Ids, uint32_t Start = 0)
{
    if (Start != Ids.Size())
    {
        qsort(Ids.GetData() + Start, Ids.Size() - Start, sizeof(AssetID), CompareAssetIDs);
    }
    uint32_t WriteIndex = Start;
    for (uint32_t Index = Start; Index < Ids.Size(); ++Index)
    {
        if (Ids[Index].IsValid() && (WriteIndex == Start || Ids[Index] != Ids[WriteIndex - 1]))
        {
            Ids[WriteIndex++] = Ids[Index];
        }
    }
    Ids.Resize(WriteIndex);
    return { Ids.GetData() + Start, WriteIndex - Start };
}

// Loaded payloads must reference exactly the dependencies the catalog reserved for them.
inline bool MatchesAssetDependencies(const AssetCatalogEntry& Entry, Array<AssetID>& Ids)
{
    const Span<AssetID> Sorted = NormalizeAssetDependencies(Ids);
    if (Sorted.Count != Entry.dependencies.Count)
    {
        return false;
    }
    for (uint32_t Index = 0; Index < Sorted.Count; ++Index)
    {
        if (Sorted.Data[Index] != Entry.dependencies.Data[Index])
        {
            return false;
        }
    }
    return true;
}
} // namespace hz
