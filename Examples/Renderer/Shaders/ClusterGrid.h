#pragma once
#ifdef __cplusplus
#include "Core/IMath.h"
#endif

// Raw buffer layout: grid, non-empty view's depth bins, global IDs, sorted finite IDs, aligned spheres.
struct ClusterGrid
{
#ifdef __cplusplus
    using float4 = Vector4;
    using uint = uint32_t;
#endif
    float4 ClipX, ClipY, ClipW;
    uint   Width, Height, TilesX, Words;
    float  Near, BinScale;
    uint   MaskIndex, GlobalCount;
    uint   IndexOffset, SphereOffset, FiniteCount, BinCount;
};
#ifdef __cplusplus
static_assert(sizeof(ClusterGrid) == 96);
#endif
#define LIGHT_TILE_SIZE 32
