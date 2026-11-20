/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IMath.h"
#include "Core/ISpan.h"

namespace hz
{
class SkeletonAsset;
class AnimationClipAsset;
class SkinningCoreData;

struct AnimationLayer
{
    const AnimationClipAsset* clip = nullptr;
    // Seconds, clamped to the clip duration.
    float                     time = 0.0f;
    float                     weight = 1.0f;
};

struct SkinBounds
{
    float minimum[3] = {};
    float maximum[3] = {};
};

// Evaluates the pose of one skeleton instance: samples and blends clips, converts the pose to the skeleton's model space
// and produces the skinning matrices and bounds of the skins bound to the skeleton. Working memory is allocated at
// construction, so evaluation does not allocate. The skeleton asset must outlive this object.
class SkinningCore
{
public:
    explicit SkinningCore(const SkeletonAsset& skeleton);
    ~SkinningCore();
    SkinningCore(const SkinningCore&) = delete;
    SkinningCore& operator=(const SkinningCore&) = delete;

    const SkeletonAsset& GetSkeleton() const;
    // Without layers, or when every weight is zero, the pose is the rest pose. Fails and keeps the previous pose when a
    // clip has a different joint count than the skeleton.
    bool                 Evaluate(Span<AnimationLayer> layers);
    // Joint transforms in the skeleton's model space, in skeleton joint order.
    Span<Matrix4>        GetModelMatrices() const;
    // Writes one matrix per skin joint, mapping bind-space vertex positions to the skeleton's model space. output holds
    // SkeletonAsset::GetSkinJointCount(skin) matrices.
    void                 WriteSkinMatrices(uint32_t skin, Matrix4* output) const;
    // Model-space bounds of the vertices the skin deforms; false when its joints influence no vertex.
    bool                 GetSkinBounds(uint32_t skin, SkinBounds& bounds) const;

private:
    SkinningCoreData* Data = nullptr;
};
} // namespace hz
