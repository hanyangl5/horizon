/* Copyright (c) 2026 Horizon */

#include "Scene/SkinningCore.h"
#include "Scene/AnimationAssets.h"
#include "AnimationAssetData.h"
#include "Core/ILog.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include <float.h>
#include <math.h>

#include "Core/IMemory.h"

namespace hz
{
class SkinningCoreData
{
public:
    explicit SkinningCoreData(const SkeletonAsset& Asset): Asset(Asset), Skeleton(Asset.Data->skeleton)
    {
        Context.Resize(Skeleton.num_joints());
        Locals.Resize((uint32_t)Skeleton.num_soa_joints());
        Models.Resize((uint32_t)Skeleton.num_joints());
        ModelMatrices.Resize((uint32_t)Skeleton.num_joints());
        RestPose();
    }

    void RestPose()
    {
        const ozz::span<const ozz::math::SoaTransform> Rest = Skeleton.joint_rest_poses();
        for (uint32_t I = 0; I < Locals.Size(); ++I)
        {
            Locals[I] = Rest[I];
        }
    }

    bool Sample(const AnimationLayer& Layer, ozz::span<ozz::math::SoaTransform> Output)
    {
        const ozz::animation::Animation& Animation = Layer.Clip->Data->animation;
        ozz::animation::SamplingJob      Job;
        Job.animation = &Animation;
        Job.context = &Context;
        Job.ratio = fminf(fmaxf(Layer.Time / Animation.duration(), 0.0f), 1.0f);
        Job.output = Output;
        return Job.Run();
    }

    const SkeletonAsset&                      Asset;
    const ozz::animation::Skeleton&           Skeleton;
    ozz::animation::SamplingJob::Context      Context;
    Array<ozz::math::SoaTransform>            Locals;
    // Sampling output of each layer when more than one layer blends.
    Array<ozz::math::SoaTransform>            Sampled;
    Array<ozz::animation::BlendingJob::Layer> BlendLayers;
    Array<ozz::math::Float4x4>                Models;
    Array<Matrix4>                            ModelMatrices;
};

SkinningCore::SkinningCore(const SkeletonAsset& Skeleton)
{
    ASSERT(Skeleton.IsValid());
    Data = MemNew(SkinningCoreData, Skeleton);
    Evaluate({});
}

SkinningCore::~SkinningCore() { MemDelete(Data); }

const SkeletonAsset& SkinningCore::GetSkeleton() const { return Data->Asset; }

bool SkinningCore::Evaluate(Span<AnimationLayer> Layers)
{
    const int JointCount = Data->Skeleton.num_joints();
    uint32_t  Active = 0;
    for (const AnimationLayer& Layer : Layers)
    {
        if (!Layer.Clip || Layer.Weight <= 0.0f)
        {
            continue;
        }
        if (Layer.Clip->Data->animation.num_tracks() != JointCount)
        {
            LOGF(Error, "Animation has %d tracks for a %d-joint skeleton", Layer.Clip->Data->animation.num_tracks(), JointCount);
            return false;
        }
        ++Active;
    }
    const uint32_t                     SoaJoints = Data->Locals.Size();
    ozz::span<ozz::math::SoaTransform> Locals(Data->Locals.GetData(), SoaJoints);
    if (!Active)
    {
        Data->RestPose();
    }
    else if (Active == 1)
    {
        for (const AnimationLayer& Layer : Layers)
        {
            if (Layer.Clip && Layer.Weight > 0.0f && !Data->Sample(Layer, Locals))
            {
                return false;
            }
        }
    }
    else
    {
        // Grows only when more layers blend than before.
        if (Data->Sampled.Size() < Active * SoaJoints)
        {
            Data->Sampled.Resize(Active * SoaJoints);
        }
        Data->BlendLayers.Resize(Active);
        uint32_t Index = 0;
        for (const AnimationLayer& Layer : Layers)
        {
            if (!Layer.Clip || Layer.Weight <= 0.0f)
            {
                continue;
            }
            ozz::span<ozz::math::SoaTransform> Output(Data->Sampled.GetData() + Index * SoaJoints, SoaJoints);
            if (!Data->Sample(Layer, Output))
            {
                return false;
            }
            Data->BlendLayers[Index].weight = Layer.Weight;
            Data->BlendLayers[Index].transform = Output;
            ++Index;
        }
        ozz::animation::BlendingJob Job;
        Job.layers = { Data->BlendLayers.GetData(), Data->BlendLayers.Size() };
        Job.rest_pose = Data->Skeleton.joint_rest_poses();
        Job.output = Locals;
        if (!Job.Run())
        {
            return false;
        }
    }
    ozz::animation::LocalToModelJob Job;
    Job.skeleton = &Data->Skeleton;
    Job.input = { Data->Locals.GetData(), SoaJoints };
    Job.output = { Data->Models.GetData(), Data->Models.Size() };
    if (!Job.Run())
    {
        return false;
    }
    for (uint32_t Joint = 0; Joint < Data->Models.Size(); ++Joint)
    {
        float Columns[4][4];
        for (uint32_t Column = 0; Column < 4; ++Column)
        {
            ozz::math::StorePtrU(Data->Models[Joint].cols[Column], Columns[Column]);
        }
        Data->ModelMatrices[Joint] = Matrix4(Vector4(Columns[0][0], Columns[0][1], Columns[0][2], Columns[0][3]),
                                             Vector4(Columns[1][0], Columns[1][1], Columns[1][2], Columns[1][3]),
                                             Vector4(Columns[2][0], Columns[2][1], Columns[2][2], Columns[2][3]),
                                             Vector4(Columns[3][0], Columns[3][1], Columns[3][2], Columns[3][3]));
    }
    return true;
}

Span<Matrix4> SkinningCore::GetModelMatrices() const { return { Data->ModelMatrices.GetData(), Data->ModelMatrices.Size() }; }

void SkinningCore::WriteSkinMatrices(uint32_t Skin, Matrix4* Output) const
{
    const SkinData& Skinning = Data->Asset.Data->skins[Skin];
    for (uint32_t I = 0; I < Skinning.joints.Size(); ++I)
    {
        Output[I] = Data->ModelMatrices[Skinning.joints[I]] * Skinning.inverseBind[I];
    }
}

bool SkinningCore::GetSkinBounds(uint32_t Skin, SkinBounds& Bounds) const
{
    const SkinData& Skinning = Data->Asset.Data->skins[Skin];
    bool            Found = false;
    for (uint32_t Axis = 0; Axis < 3; ++Axis)
    {
        Bounds.Minimum[Axis] = FLT_MAX;
        Bounds.Maximum[Axis] = -FLT_MAX;
    }
    // Each box holds the influenced vertices in the joint's space; a blended vertex lies within the union of its joints'
    // boxes, so the union of the transformed boxes bounds the skinned mesh.
    for (uint32_t I = 0; I < Skinning.joints.Size(); ++I)
    {
        const float* Box = Skinning.bounds.GetData() + I * 6;
        if (Box[0] > Box[3])
        {
            continue;
        }
        Found = true;
        const Matrix4& Matrix = Data->ModelMatrices[Skinning.joints[I]];
        for (uint32_t Row = 0; Row < 3; ++Row)
        {
            float Minimum = Matrix.getElem(3, (int)Row), Maximum = Minimum;
            for (uint32_t Column = 0; Column < 3; ++Column)
            {
                const float A = Matrix.getElem((int)Column, (int)Row) * Box[Column];
                const float B = Matrix.getElem((int)Column, (int)Row) * Box[3 + Column];
                Minimum += fminf(A, B);
                Maximum += fmaxf(A, B);
            }
            Bounds.Minimum[Row] = fminf(Bounds.Minimum[Row], Minimum);
            Bounds.Maximum[Row] = fmaxf(Bounds.Maximum[Row], Maximum);
        }
    }
    return Found;
}
} // namespace hz
