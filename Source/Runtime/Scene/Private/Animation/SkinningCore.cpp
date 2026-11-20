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
    explicit SkinningCoreData(const SkeletonAsset& asset): asset(asset), skeleton(asset.Data->skeleton)
    {
        context.Resize(skeleton.num_joints());
        locals.Resize((uint32_t)skeleton.num_soa_joints());
        models.Resize((uint32_t)skeleton.num_joints());
        modelMatrices.Resize((uint32_t)skeleton.num_joints());
        restPose();
    }

    void restPose()
    {
        const ozz::span<const ozz::math::SoaTransform> rest = skeleton.joint_rest_poses();
        for (uint32_t i = 0; i < locals.Size(); ++i)
            locals[i] = rest[i];
    }

    bool sample(const AnimationLayer& layer, ozz::span<ozz::math::SoaTransform> output)
    {
        const ozz::animation::Animation& animation = layer.clip->Data->animation;
        ozz::animation::SamplingJob      job;
        job.animation = &animation;
        job.context = &context;
        job.ratio = fminf(fmaxf(layer.time / animation.duration(), 0.0f), 1.0f);
        job.output = output;
        return job.Run();
    }

    const SkeletonAsset&                   asset;
    const ozz::animation::Skeleton&         skeleton;
    ozz::animation::SamplingJob::Context    context;
    Array<ozz::math::SoaTransform>          locals;
    // Sampling output of each layer when more than one layer blends.
    Array<ozz::math::SoaTransform>          sampled;
    Array<ozz::animation::BlendingJob::Layer> blendLayers;
    Array<ozz::math::Float4x4>              models;
    Array<Matrix4>                          modelMatrices;
};

SkinningCore::SkinningCore(const SkeletonAsset& skeleton)
{
    ASSERT(skeleton.IsValid());
    Data = MemNew(SkinningCoreData, skeleton);
    Evaluate({});
}

SkinningCore::~SkinningCore() { MemDelete(Data); }

const SkeletonAsset& SkinningCore::GetSkeleton() const { return Data->asset; }

bool SkinningCore::Evaluate(Span<AnimationLayer> layers)
{
    const int jointCount = Data->skeleton.num_joints();
    uint32_t  active = 0;
    for (const AnimationLayer& layer : layers)
    {
        if (!layer.clip || layer.weight <= 0.0f)
            continue;
        if (layer.clip->Data->animation.num_tracks() != jointCount)
        {
            LOGF(Error, "Animation has %d tracks for a %d-joint skeleton", layer.clip->Data->animation.num_tracks(), jointCount);
            return false;
        }
        ++active;
    }
    const uint32_t                     soaJoints = Data->locals.Size();
    ozz::span<ozz::math::SoaTransform> locals(Data->locals.GetData(), soaJoints);
    if (!active)
        Data->restPose();
    else if (active == 1)
    {
        for (const AnimationLayer& layer : layers)
            if (layer.clip && layer.weight > 0.0f && !Data->sample(layer, locals))
                return false;
    }
    else
    {
        // Grows only when more layers blend than before.
        if (Data->sampled.Size() < active * soaJoints)
            Data->sampled.Resize(active * soaJoints);
        Data->blendLayers.Resize(active);
        uint32_t index = 0;
        for (const AnimationLayer& layer : layers)
        {
            if (!layer.clip || layer.weight <= 0.0f)
                continue;
            ozz::span<ozz::math::SoaTransform> output(Data->sampled.GetData() + index * soaJoints, soaJoints);
            if (!Data->sample(layer, output))
                return false;
            Data->blendLayers[index].weight = layer.weight;
            Data->blendLayers[index].transform = output;
            ++index;
        }
        ozz::animation::BlendingJob job;
        job.layers = { Data->blendLayers.GetData(), Data->blendLayers.Size() };
        job.rest_pose = Data->skeleton.joint_rest_poses();
        job.output = locals;
        if (!job.Run())
            return false;
    }
    ozz::animation::LocalToModelJob job;
    job.skeleton = &Data->skeleton;
    job.input = { Data->locals.GetData(), soaJoints };
    job.output = { Data->models.GetData(), Data->models.Size() };
    if (!job.Run())
        return false;
    for (uint32_t joint = 0; joint < Data->models.Size(); ++joint)
    {
        float columns[4][4];
        for (uint32_t column = 0; column < 4; ++column)
            ozz::math::StorePtrU(Data->models[joint].cols[column], columns[column]);
        Data->modelMatrices[joint] =
            Matrix4(Vector4(columns[0][0], columns[0][1], columns[0][2], columns[0][3]),
                    Vector4(columns[1][0], columns[1][1], columns[1][2], columns[1][3]),
                    Vector4(columns[2][0], columns[2][1], columns[2][2], columns[2][3]),
                    Vector4(columns[3][0], columns[3][1], columns[3][2], columns[3][3]));
    }
    return true;
}

Span<Matrix4> SkinningCore::GetModelMatrices() const { return { Data->modelMatrices.GetData(), Data->modelMatrices.Size() }; }

void SkinningCore::WriteSkinMatrices(uint32_t skin, Matrix4* output) const
{
    const SkinData& data = Data->asset.Data->skins[skin];
    for (uint32_t i = 0; i < data.joints.Size(); ++i)
        output[i] = Data->modelMatrices[data.joints[i]] * data.inverseBind[i];
}

bool SkinningCore::GetSkinBounds(uint32_t skin, SkinBounds& bounds) const
{
    const SkinData& data = Data->asset.Data->skins[skin];
    bool            found = false;
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
        bounds.minimum[axis] = FLT_MAX;
        bounds.maximum[axis] = -FLT_MAX;
    }
    // Each box holds the influenced vertices in the joint's space; a blended vertex lies within the union of its joints'
    // boxes, so the union of the transformed boxes bounds the skinned mesh.
    for (uint32_t i = 0; i < data.joints.Size(); ++i)
    {
        const float* box = data.bounds.GetData() + i * 6;
        if (box[0] > box[3])
            continue;
        found = true;
        const Matrix4& matrix = Data->modelMatrices[data.joints[i]];
        for (uint32_t row = 0; row < 3; ++row)
        {
            float minimum = matrix.getElem(3, (int)row), maximum = minimum;
            for (uint32_t column = 0; column < 3; ++column)
            {
                const float a = matrix.getElem((int)column, (int)row) * box[column];
                const float b = matrix.getElem((int)column, (int)row) * box[3 + column];
                minimum += fminf(a, b);
                maximum += fmaxf(a, b);
            }
            bounds.minimum[row] = fminf(bounds.minimum[row], minimum);
            bounds.maximum[row] = fmaxf(bounds.maximum[row], maximum);
        }
    }
    return found;
}
} // namespace hz
