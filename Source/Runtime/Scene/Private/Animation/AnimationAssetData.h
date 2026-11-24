/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"

namespace hz
{
struct SkinData
{
    // Per skin joint: the skeleton joint it follows, its inverse bind matrix and the bind-space bounds of the vertices
    // it influences. A joint that influences no vertex has a minimum above its maximum.
    Array<uint32_t> joints;
    Array<Matrix4>  inverseBind;
    Array<float>    bounds;
};

class SkeletonAssetData
{
public:
    ozz::animation::Skeleton skeleton;
    Array<SkinData>          skins;
};

class AnimationClipAssetData
{
public:
    ozz::animation::Animation animation;
};
} // namespace hz
