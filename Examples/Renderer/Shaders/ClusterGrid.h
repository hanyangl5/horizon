#pragma once
#ifdef __cplusplus
#include <stdint.h>
#endif

#define LIGHT_TILE_SIZE          32
#define CLUSTER_MAX_LIGHTS       4096u
#define CLUSTER_DEPTH_BIN_COUNT  1024u
#define LIGHTS_PER_BITMASK       32u
#define LIGHT_MASK_GROUP_SIZE_X  8
#define LIGHT_MASK_GROUP_SIZE_Y  4

#define TILE_LIGHT_BITMASK_COUNT ((CLUSTER_MAX_LIGHTS + LIGHTS_PER_BITMASK - 1) / LIGHTS_PER_BITMASK)

// Each thread loads one sphere for one bit.
#ifdef __cplusplus
static_assert(LIGHT_MASK_GROUP_SIZE_X * LIGHT_MASK_GROUP_SIZE_Y == LIGHTS_PER_BITMASK);
static_assert(LIGHTS_PER_BITMASK == sizeof(uint32_t) * 8);
#endif

#ifndef __cplusplus
uint GetTileLightMaskBase(uint2 Tile, uint TilesX) { return (Tile.y * TilesX + Tile.x) * TILE_LIGHT_BITMASK_COUNT; }
#endif
