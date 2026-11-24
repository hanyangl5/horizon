/* Copyright (c) 2026 Horizon */
#pragma once

#include <stdint.h>

// Cooked skeleton and animation files, written by the AssetPipeline and read by the animation loaders. Both end with an
// ozz archive, which carries its own type tags and versions.
namespace hz
{
// Skeleton file: header, then skinCount skins, then archiveSize bytes of ozz skeleton archive.
// Each skin: jointCount (uint32), then per joint the ozz joint index (uint32), then per joint the column-major inverse
// bind matrix (16 floats), then per joint the bounds of the vertices it influences in joint space (min xyz, max xyz).
// A joint that influences no vertex has a minimum above its maximum.
struct SkeletonFileHeader
{
    uint32_t magic = 0x4B535A48; // "HZSK"
    uint32_t version = 1;
    uint32_t skinCount = 0;
    uint32_t archiveSize = 0;
};

// Animation file: header, then archiveSize bytes of ozz animation archive with one track per skeleton joint.
struct AnimationFileHeader
{
    uint32_t magic = 0x4E415A48; // "HZAN"
    uint32_t version = 1;
    uint32_t archiveSize = 0;
    uint32_t reserved = 0;
};

constexpr uint32_t kMaxSkinJoints = UINT16_MAX;
} // namespace hz
