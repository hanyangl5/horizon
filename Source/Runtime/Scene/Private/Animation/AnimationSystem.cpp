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
    Entity        Target;
    AssetID       SkeletonID;
    AssetHandle   Skeleton;
    AssetID       ClipID;
    AssetHandle   Clip;
    SkinningCore* Pose = nullptr;
    bool          ReportedMismatch = false;
    bool          Seen = false;
};

struct AnimatedEntityIndex
{
    Entity   key;
    uint32_t value;
};

class AnimationSystemData
{
public:
    AnimationSystemData(World& World, AssetManager& AssetManager):
        SceneWorld(World), Assets(AssetManager), Query(World, { SceneTypes::skeleton })
    {
        ASSERT(Query.IsValid());
    }
    ~AnimationSystemData()
    {
        for (AnimatedEntity& Entity : Entities)
            Release(Entity);
        hmfree(Indices);
    }

    void Release(AnimatedEntity& Entity)
    {
        MemDelete(Entity.Pose);
        Entity.Pose = nullptr;
        if (Entity.Skeleton.IsValid())
            Assets.Release(Entity.Skeleton);
        if (Entity.Clip.IsValid())
            Assets.Release(Entity.Clip);
        Entity.Skeleton = {};
        Entity.Clip = {};
    }

    // A changed ID releases the previous asset; the new one is requested and becomes usable once ready.
    void Retain(AssetID Id, AssetID& CurrentID, AssetHandle& Handle)
    {
        if (CurrentID == Id)
            return;
        if (Handle.IsValid())
            Assets.Release(Handle);
        CurrentID = Id;
        Handle = Id.IsValid() ? Assets.Request(Id) : AssetHandle{};
    }

    static bool Sync(Entity Entity, Span<const void*> Fields, void* UserData)
    {
        AnimationSystemData&       Data = *(AnimationSystemData*)UserData;
        const AnimatedEntityIndex* Entry = hmgetp_null(Data.Indices, Entity);
        if (!Entry)
        {
            AnimatedEntity& Added = Data.Entities.EmplaceBack();
            Added.Target = Entity;
            const AnimatedEntityIndex Index = { .key = Entity, .value = Data.Entities.Size() - 1 };
            hmputs(Data.Indices, Index);
            Entry = hmgetp_null(Data.Indices, Entity);
        }
        AnimatedEntity& Animated = Data.Entities[Entry->value];
        Animated.Seen = true;
        const Skeleton&        Component = *(const Skeleton*)Fields.Data[0];
        const AnimationPlayer* Player = (const AnimationPlayer*)Data.SceneWorld.GetComponent(Entity, SceneTypes::animationPlayer);
        if (Animated.SkeletonID != Component.asset)
        {
            MemDelete(Animated.Pose);
            Animated.Pose = nullptr;
            Animated.ReportedMismatch = false;
        }
        Data.Retain(Component.asset, Animated.SkeletonID, Animated.Skeleton);
        if (Player && Animated.ClipID != Player->clip)
            Animated.ReportedMismatch = false;
        Data.Retain(Player ? Player->clip : AssetID{}, Animated.ClipID, Animated.Clip);
        Data.Animate(Animated, Player);
        return true;
    }

    void Animate(AnimatedEntity& Animated, const AnimationPlayer* Player)
    {
        const SkeletonAsset* Skeleton = Assets.GetSkeleton(Animated.Skeleton);
        if (!Skeleton)
            return;
        if (!Animated.Pose)
            Animated.Pose = MemNew(SkinningCore, *Skeleton);
        const AnimationClipAsset* Clip = Assets.GetAnimationClip(Animated.Clip);
        AnimationLayer            Layer;
        if (Player && Clip)
        {
            AnimationPlayer Advanced = *Player;
            const float     Duration = Clip->GetDuration();
            if (Advanced.playing && Advanced.speed != 0.0f)
            {
                Advanced.time += DeltaSeconds * Advanced.speed;
                if (Advanced.loop)
                    Advanced.time -= floorf(Advanced.time / Duration) * Duration;
                else if (Advanced.time <= 0.0f || Advanced.time >= Duration)
                {
                    Advanced.time = fminf(fmaxf(Advanced.time, 0.0f), Duration);
                    Advanced.playing = false;
                }
                const bool Updated = SceneWorld.SetComponent(Animated.Target, SceneTypes::animationPlayer, &Advanced);
                ASSERT(Updated);
            }
            Layer = { .Clip = Clip, .Time = Advanced.time };
        }
        if (!Animated.Pose->Evaluate({ &Layer, Layer.Clip ? 1u : 0u }) && !Animated.ReportedMismatch)
        {
            Animated.ReportedMismatch = true;
            LOGF(Error, "Animation clip does not match the skeleton of its entity; the skeleton keeps its last pose");
        }
    }

    void Update(float Elapsed)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("AnimationSystem::Update");
        ASSERT(!SceneWorld.IsDeferred());
        DeltaSeconds = Elapsed;
        for (AnimatedEntity& Entity : Entities)
            Entity.Seen = false;
        Query.Each(Sync, this);
        for (uint32_t I = 0; I < Entities.Size();)
        {
            if (Entities[I].Seen)
            {
                ++I;
                continue;
            }
            Release(Entities[I]);
            hmdel(Indices, Entities[I].Target);
            if (I + 1 < Entities.Size())
            {
                Entities[I] = Entities[Entities.Size() - 1];
                hmgetp_null(Indices, Entities[I].Target)->value = I;
            }
            Entities.PopBack();
        }
    }

    World&                SceneWorld;
    AssetManager&         Assets;
    WorldQuery            Query;
    Array<AnimatedEntity> Entities;
    AnimatedEntityIndex*  Indices = nullptr;
    float                 DeltaSeconds = 0.0f;
};

AnimationSystem::AnimationSystem(World& World, AssetManager& AssetManager): Data(MemNew(AnimationSystemData, World, AssetManager)) {}
AnimationSystem::~AnimationSystem() { MemDelete(Data); }
void                AnimationSystem::Update(float DeltaSeconds) { Data->Update(DeltaSeconds); }
const SkinningCore* AnimationSystem::GetPose(Entity Entity) const
{
    const AnimatedEntityIndex* Entry = hmgetp_null(Data->Indices, Entity);
    return Entry ? Data->Entities[Entry->value].Pose : nullptr;
}
} // namespace hz
