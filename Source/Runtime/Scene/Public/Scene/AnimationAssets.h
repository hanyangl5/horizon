/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ISpan.h"

namespace hz
{
class SkeletonAssetData;
class AnimationClipAssetData;

// A cooked joint hierarchy and the skins bound to it. Each skin maps its joints to skeleton joints and stores their
// inverse bind matrices.
class SkeletonAsset
{
public:
    SkeletonAsset() = default;
    ~SkeletonAsset();
    SkeletonAsset(const SkeletonAsset&) = delete;
    SkeletonAsset& operator=(const SkeletonAsset&) = delete;

    // Decodes a cooked skeleton file. A failed parse keeps the previous contents.
    bool     Parse(Span<char> bytes);
    bool     IsValid() const { return Data != nullptr; }
    uint32_t GetJointCount() const;
    uint32_t GetSkinCount() const;
    uint32_t GetSkinJointCount(uint32_t skin) const;

private:
    SkeletonAssetData* Data = nullptr;
    friend class SkinningCore;
};

// A cooked animation with one track per joint of its skeleton.
class AnimationClipAsset
{
public:
    AnimationClipAsset() = default;
    ~AnimationClipAsset();
    AnimationClipAsset(const AnimationClipAsset&) = delete;
    AnimationClipAsset& operator=(const AnimationClipAsset&) = delete;

    bool     Parse(Span<char> bytes);
    bool     IsValid() const { return Data != nullptr; }
    // Seconds.
    float    GetDuration() const;
    uint32_t GetTrackCount() const;

private:
    AnimationClipAssetData* Data = nullptr;
    friend class SkinningCore;
};
} // namespace hz
