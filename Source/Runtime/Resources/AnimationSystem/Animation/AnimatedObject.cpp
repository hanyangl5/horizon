/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "AnimatedObject.h"

#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/ik_aim_job.h"
#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/ik_two_bone_job.h"

#include "Core/IMemory.h"

namespace
{
void MultiplySoATransformQuaternion(int32_t _index, const Quat& _quat, ozz::span<SoaTransform>& _transforms)
{
    SoaTransform& soa_transform_ref = _transforms[_index / 4];
    Vector4       aos_quats[4];

    transpose4x4(&soa_transform_ref.rotation.x, aos_quats);

    Vector4& aos_quat_ref = aos_quats[_index & 3];
    aos_quat_ref = Vector4((Quat(aos_quat_ref) * _quat));

    transpose4x4(aos_quats, &soa_transform_ref.rotation.x);
}
} // namespace

void AnimatedObject::Initialize(Rig* rig, Animation* animation)
{
    this->rig = rig;
    this->animation = animation;

    COMPILE_ASSERT(alignof(Matrix4) == alignof(Vector3) && alignof(Matrix4) == alignof(SoaTransform));
    uint32_t totalSize = sizeof(Matrix4) * 2 * rig->numJoints + sizeof(SoaTransform) * rig->numSoaJoints;

#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    totalSize += (sizeof(Vector3)) * rig->numJoints;
#endif

    void* alloc = tf_memalign(alignof(Matrix4), totalSize);

    jointWorldMats = ozz::span<Matrix4>((Matrix4*)alloc, rig->numJoints);
    jointModelMats = ozz::span<Matrix4>(jointWorldMats.data() + rig->numJoints, rig->numJoints);
    localTrans = ozz::span<SoaTransform>((SoaTransform*)(jointModelMats.data() + rig->numJoints), rig->numSoaJoints); //-V1027

#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    jointScales = ozz::span<Vector3>((Vector3*)(localTrans.data() + rig->numSoaJoints), rig->numJoints); //-V1027
#endif

    for (uint32_t i = 0; i < rig->numJoints; ++i)
    {
        jointWorldMats[i] = Matrix4::identity();
        jointModelMats[i] = Matrix4::identity();

#ifdef ENABLE_FORGE_ANIMATION_DEBUG
        jointScales[i] = Vector3::one();
#endif
    }

    for (uint32_t i = 0; i < rig->numSoaJoints; ++i)
    {
        localTrans[i] = SoaTransform::identity();
    }
}

void AnimatedObject::Exit()
{
    tf_free(jointWorldMats.data());

    jointModelMats = {};
    jointWorldMats = {};
    localTrans = {};

#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    jointScales = {};
#endif

    rig = NULL;
    animation = NULL;
}

void AnimatedObject::ComputePose(const Matrix4& rootTransform)
{
    // Set the world matrix of each joint
    for (uint32_t jointIndex = 0; jointIndex < rig->numJoints; jointIndex++)
    {
        jointWorldMats[jointIndex] = rootTransform * jointModelMats[jointIndex];
    }
}

// compute joint scales
void AnimatedObject::ComputeJointScales(const Matrix4& rootTransform)
{
    UNREF_PARAM(rootTransform);
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // Traverses through the skeleton's joint hierarchy, placing bones between
    // joints and altering the size of joints and bones to reflect distances
    // between joints

    // Store smallest bone lenth to be reused for root joint scale
    float minBoneLen = 0.f;
    bool  minBoneLenSet = false;

    // For each joint
    for (uint32_t childIndex = 0; childIndex < rig->numJoints; childIndex++)
    {
        // Do not make a bone if it is the root
        // Handle the root joint specially after the loop
        if (childIndex == rig->rootIndex)
        {
            continue;
        }

        // Get the index of the parent of childIndex
        const int32_t parentIndex = rig->skeleton.joint_parents()[childIndex];

        // Selects joint matrices.
        const mat4 parentMat = jointModelMats[parentIndex];
        const mat4 childMat = jointModelMats[childIndex];

        vec3  boneDir = childMat.getCol3().getXYZ() - parentMat.getCol3().getXYZ();
        float boneLen = length(boneDir);

        // Save smallest boneLen for the root joints scale size
        if ((!minBoneLenSet) || (boneLen < minBoneLen))
        {
            minBoneLen = boneLen;
            minBoneLenSet = true;
        }

        // Sets the scale of the joint equivilant to the boneLen between it and its parent joint
        // Separete from world so outside objects can use a joint's world mat w/o its scale
        jointScales[childIndex] = vec3(boneLen / 2.0f);
    }

    // Set the root joints scale based on the saved min value
    jointScales[rig->rootIndex] = vec3(minBoneLen / 2.0f);
#endif
}

bool AnimatedObject::Update(float dt)
{
    // sample the current animation to get localTrans
    if (!animation->Sample(dt, localTrans))
        return false;

    // Local to model job

    // Setup local-to-model conversion job.
    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = &rig->skeleton;
    ltmJob.input = localTrans;
    ltmJob.output = jointModelMats;

    // Runs ltm job.
    if (!ltmJob.Run())
        return false;

    return true;
}

bool AnimatedObject::AimIK(AimIKDesc* params, const Point3& target)
{
    ozz::span<Matrix4> models = jointModelMats;

    ozz::animation::IKAimJob ikJob;
    ikJob.pole_vector = params->poleVector;
    ikJob.twist_angle = params->twistAngle;
    ikJob.target = target;
    ikJob.reached = &params->reached;

    Quat correction;
    ikJob.joint_correction = &correction;

    int32_t previous_joint = ozz::animation::Skeleton::kNoParent;
    for (int32_t i = 0, joint = params->jointChain[0]; i < params->jointChainLength;
         ++i, previous_joint = joint, joint = params->jointChain[i])
    {
        ikJob.joint = &models[joint];
        ikJob.up = params->jointUpVectors[i];

        const bool last = i == params->jointChainLength - 1;
        ikJob.weight = last ? 1.f : params->jointWeight;

        if (i == 0)
        {
            ikJob.offset = params->offset;
            ikJob.forward = params->forward;
        }
        else
        {
            const Vector3 corrected_forward_ms((models[previous_joint] * rotate(correction, ikJob.forward)).getXYZ());
            const Point3  corrected_offset_ms((models[previous_joint] * Point3(rotate(correction, ikJob.offset))).getXYZ());

            Matrix4 inv_joint = inverse(models[joint]);
            ikJob.forward = (inv_joint * corrected_forward_ms).getXYZ();
            ikJob.offset = (inv_joint * corrected_offset_ms).getXYZ();
        }

        if (!ikJob.Run())
        {
            return false;
        }

        MultiplySoATransformQuaternion(joint, correction, localTrans);
    }

    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = &rig->skeleton;
    ltmJob.input = localTrans;
    ltmJob.output = jointModelMats;

    if (!ltmJob.Run())
        return false;

    return true;
}

bool AnimatedObject::TwoBonesIK(TwoBonesIKDesc* params, const Point3& target)
{
    ozz::animation::IKTwoBoneJob ik_job;

    ik_job.target = target;
    ik_job.pole_vector = params->poleVector;
    ik_job.mid_axis = params->midAxis;

    ik_job.weight = params->weight;
    ik_job.soften = params->soften;
    ik_job.twist_angle = params->twistAngle;

    ozz::span<Matrix4> models = jointModelMats;

    ik_job.start_joint = &models[params->jointChain[0]];
    ik_job.mid_joint = &models[params->jointChain[1]];
    ik_job.end_joint = &models[params->jointChain[2]];

    // Outputs
    Quat start_correction;
    ik_job.start_joint_correction = &start_correction;
    Quat mid_correction;
    ik_job.mid_joint_correction = &mid_correction;
    ik_job.reached = &params->reached;

    if (!ik_job.Run())
    {
        return false;
    }

    MultiplySoATransformQuaternion(params->jointChain[0], start_correction, localTrans);
    MultiplySoATransformQuaternion(params->jointChain[1], mid_correction, localTrans);

    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = &rig->skeleton;
    ltmJob.input = localTrans;
    ltmJob.output = jointModelMats;

    // Runs ltm job.
    if (!ltmJob.Run())
        return false;

    return true;
}

void AnimatedObject::ComputeBindPose(const Matrix4& rootTransform)
{
    // Setup local-to-model conversion job.
    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = &rig->skeleton;
    ltmJob.input = rig->skeleton.joint_rest_poses(); // Use the skeleton's bind pose
    ltmJob.output = jointModelMats;

    // Runs ltm job.
    if (ltmJob.Run())
        ComputePose(rootTransform);
}