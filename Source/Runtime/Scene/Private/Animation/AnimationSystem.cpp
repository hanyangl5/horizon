/* Copyright (c) 2026 Horizon */

#include "Scene/AnimationSystem.h"
#include "Scene/AnimationAssets.h"
#include "Scene/AssetManager.h"
#include "Scene/SceneComponents.h"
#include "Scene/SkinningCore.h"
#include "Core/IContainer.h"
#include "Profiler/IProfiler.h"
#include <math.h>

#include "Core/IMemory.h"

namespace hz
{
struct AnimatedEntity
{
    Entity        entity;
    AssetID       skeletonID;
    AssetHandle   skeleton;
    AssetID       clipID;
    AssetHandle   clip;
    SkinningCore* pose = nullptr;
    bool          reportedMismatch = false;
    bool          seen = false;
};

struct AnimatedEntityIndex
{
    Entity   key;
    uint32_t value;
};

class AnimationSystemData
{
public:
    AnimationSystemData(World& world, AssetManager& assetManager):
        world(world), assetManager(assetManager), query(world, { SceneTypes::skeleton })
    {
        ASSERT(query.IsValid());
    }
    ~AnimationSystemData()
    {
        for (AnimatedEntity& entity : entities)
            release(entity);
        hmfree(indices);
    }

    void release(AnimatedEntity& entity)
    {
        MemDelete(entity.pose);
        entity.pose = nullptr;
        if (entity.skeleton.IsValid())
            assetManager.Release(entity.skeleton);
        if (entity.clip.IsValid())
            assetManager.Release(entity.clip);
        entity.skeleton = {};
        entity.clip = {};
    }

    // A changed ID releases the previous asset; the new one is requested and becomes usable once ready.
    void retain(AssetID id, AssetID& currentID, AssetHandle& handle)
    {
        if (currentID == id)
            return;
        if (handle.IsValid())
            assetManager.Release(handle);
        currentID = id;
        handle = id.IsValid() ? assetManager.Request(id) : AssetHandle{};
    }

    static bool sync(Entity entity, Span<const void*> fields, void* userData)
    {
        AnimationSystemData&       data = *(AnimationSystemData*)userData;
        const AnimatedEntityIndex* entry = hmgetp_null(data.indices, entity);
        if (!entry)
        {
            AnimatedEntity& added = data.entities.EmplaceBack();
            added.entity = entity;
            const AnimatedEntityIndex index = { .key = entity, .value = data.entities.Size() - 1 };
            hmputs(data.indices, index);
            entry = hmgetp_null(data.indices, entity);
        }
        AnimatedEntity& animated = data.entities[entry->value];
        animated.seen = true;
        const Skeleton&        skeleton = *(const Skeleton*)fields.Data[0];
        const AnimationPlayer* player = (const AnimationPlayer*)data.world.GetComponent(entity, SceneTypes::animationPlayer);
        if (animated.skeletonID != skeleton.asset)
        {
            MemDelete(animated.pose);
            animated.pose = nullptr;
            animated.reportedMismatch = false;
        }
        data.retain(skeleton.asset, animated.skeletonID, animated.skeleton);
        if (player && animated.clipID != player->clip)
            animated.reportedMismatch = false;
        data.retain(player ? player->clip : AssetID{}, animated.clipID, animated.clip);
        data.animate(animated, player);
        return true;
    }

    void animate(AnimatedEntity& animated, const AnimationPlayer* player)
    {
        const SkeletonAsset* skeleton = assetManager.GetSkeleton(animated.skeleton);
        if (!skeleton)
            return;
        if (!animated.pose)
            animated.pose = MemNew(SkinningCore, *skeleton);
        const AnimationClipAsset* clip = assetManager.GetAnimationClip(animated.clip);
        AnimationLayer            layer;
        if (player && clip)
        {
            AnimationPlayer advanced = *player;
            const float     duration = clip->GetDuration();
            if (advanced.playing && advanced.speed != 0.0f)
            {
                advanced.time += deltaSeconds * advanced.speed;
                if (advanced.loop)
                    advanced.time -= floorf(advanced.time / duration) * duration;
                else if (advanced.time <= 0.0f || advanced.time >= duration)
                {
                    advanced.time = fminf(fmaxf(advanced.time, 0.0f), duration);
                    advanced.playing = false;
                }
                const bool updated = world.SetComponent(animated.entity, SceneTypes::animationPlayer, &advanced);
                ASSERT(updated);
            }
            layer = { .clip = clip, .time = advanced.time };
        }
        if (!animated.pose->Evaluate({ &layer, layer.clip ? 1u : 0u }) && !animated.reportedMismatch)
        {
            animated.reportedMismatch = true;
            LOGF(Error, "Animation clip does not match the skeleton of its entity; the skeleton keeps its last pose");
        }
    }

    void Update(float elapsed)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AnimationSystem::Update");
        ASSERT(!world.IsDeferred());
        deltaSeconds = elapsed;
        for (AnimatedEntity& entity : entities)
            entity.seen = false;
        query.Each(sync, this);
        for (uint32_t i = 0; i < entities.Size();)
        {
            if (entities[i].seen)
            {
                ++i;
                continue;
            }
            release(entities[i]);
            hmdel(indices, entities[i].entity);
            if (i + 1 < entities.Size())
            {
                entities[i] = entities[entities.Size() - 1];
                hmgetp_null(indices, entities[i].entity)->value = i;
            }
            entities.PopBack();
        }
    }

    World&                world;
    AssetManager&         assetManager;
    WorldQuery            query;
    Array<AnimatedEntity> entities;
    AnimatedEntityIndex*  indices = nullptr;
    float                 deltaSeconds = 0.0f;
};

AnimationSystem::AnimationSystem(World& world, AssetManager& assetManager): Data(MemNew(AnimationSystemData, world, assetManager)) {}
AnimationSystem::~AnimationSystem() { MemDelete(Data); }
void                AnimationSystem::Update(float deltaSeconds) { Data->Update(deltaSeconds); }
const SkinningCore* AnimationSystem::GetPose(Entity entity) const
{
    const AnimatedEntityIndex* entry = hmgetp_null(Data->indices, entity);
    return entry ? Data->entities[entry->value].pose : nullptr;
}
} // namespace hz
