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
    SkinningCore* pPose = nullptr;
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
        hmfree(pIndices);
    }

    void release(AnimatedEntity& entity)
    {
        MemDelete(entity.pPose);
        entity.pPose = nullptr;
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

    static bool sync(Entity entity, Span<const void*> fields, void* pUser)
    {
        AnimationSystemData&       data = *(AnimationSystemData*)pUser;
        const AnimatedEntityIndex* pIndex = hmgetp_null(data.pIndices, entity);
        if (!pIndex)
        {
            AnimatedEntity& added = data.entities.EmplaceBack();
            added.entity = entity;
            const AnimatedEntityIndex index = { .key = entity, .value = data.entities.Size() - 1 };
            hmputs(data.pIndices, index);
            pIndex = hmgetp_null(data.pIndices, entity);
        }
        AnimatedEntity& animated = data.entities[pIndex->value];
        animated.seen = true;
        const Skeleton&        skeleton = *(const Skeleton*)fields.Data[0];
        const AnimationPlayer* pPlayer = (const AnimationPlayer*)data.world.GetComponent(entity, SceneTypes::animationPlayer);
        if (animated.skeletonID != skeleton.asset)
        {
            MemDelete(animated.pPose);
            animated.pPose = nullptr;
            animated.reportedMismatch = false;
        }
        data.retain(skeleton.asset, animated.skeletonID, animated.skeleton);
        if (pPlayer && animated.clipID != pPlayer->clip)
            animated.reportedMismatch = false;
        data.retain(pPlayer ? pPlayer->clip : AssetID{}, animated.clipID, animated.clip);
        data.animate(animated, pPlayer);
        return true;
    }

    void animate(AnimatedEntity& animated, const AnimationPlayer* pPlayer)
    {
        const SkeletonAsset* pSkeleton = assetManager.GetSkeleton(animated.skeleton);
        if (!pSkeleton)
            return;
        if (!animated.pPose)
            animated.pPose = MemNew(SkinningCore, *pSkeleton);
        const AnimationClipAsset* pClip = assetManager.GetAnimationClip(animated.clip);
        AnimationLayer            layer;
        if (pPlayer && pClip)
        {
            AnimationPlayer player = *pPlayer;
            const float     duration = pClip->GetDuration();
            if (player.playing && player.speed != 0.0f)
            {
                player.time += deltaSeconds * player.speed;
                if (player.loop)
                    player.time -= floorf(player.time / duration) * duration;
                else if (player.time <= 0.0f || player.time >= duration)
                {
                    player.time = fminf(fmaxf(player.time, 0.0f), duration);
                    player.playing = false;
                }
                const bool updated = world.SetComponent(animated.entity, SceneTypes::animationPlayer, &player);
                ASSERT(updated);
            }
            layer = { .clip = pClip, .time = player.time };
        }
        if (!animated.pPose->Evaluate({ &layer, layer.clip ? 1u : 0u }) && !animated.reportedMismatch)
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
            hmdel(pIndices, entities[i].entity);
            if (i + 1 < entities.Size())
            {
                entities[i] = entities[entities.Size() - 1];
                hmgetp_null(pIndices, entities[i].entity)->value = i;
            }
            entities.PopBack();
        }
    }

    World&                world;
    AssetManager&         assetManager;
    WorldQuery            query;
    Array<AnimatedEntity> entities;
    AnimatedEntityIndex*  pIndices = nullptr;
    float                 deltaSeconds = 0.0f;
};

AnimationSystem::AnimationSystem(World& world, AssetManager& assetManager): Data(MemNew(AnimationSystemData, world, assetManager)) {}
AnimationSystem::~AnimationSystem() { MemDelete(Data); }
void                AnimationSystem::Update(float deltaSeconds) { Data->Update(deltaSeconds); }
const SkinningCore* AnimationSystem::GetPose(Entity entity) const
{
    const AnimatedEntityIndex* pIndex = hmgetp_null(Data->pIndices, entity);
    return pIndex ? Data->entities[pIndex->value].pPose : nullptr;
}
} // namespace hz
