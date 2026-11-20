/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/World.h"

namespace hz
{
class AssetManager;
class SkinningCore;
class AnimationSystemData;

// Plays the AnimationPlayer of every entity with a Skeleton and keeps its pose. Holds the skeleton and clip assets those
// entities reference. World and asset manager must outlive this object.
class AnimationSystem
{
public:
    AnimationSystem(World& world, AssetManager& assetManager);
    ~AnimationSystem();
    AnimationSystem(const AnimationSystem&) = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    // Advances playback, writing the time back to each AnimationPlayer, and evaluates the poses. Run it after the asset
    // manager update and before the render scene extracts the frame.
    void                Update(float deltaSeconds);
    // The pose of a Skeleton entity, valid until the next Update; null until its skeleton is loaded.
    const SkinningCore* GetPose(Entity entity) const;

private:
    AnimationSystemData* Data = nullptr;
};
} // namespace hz
