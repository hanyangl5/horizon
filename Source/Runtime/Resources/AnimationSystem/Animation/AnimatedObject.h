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

#pragma once

#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/blending_job.h"
#include "../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/runtime/local_to_model_job.h"

#include "Core/IMath.h"

#include "Animation.h"
#include "Rig.h"

struct AimIKDesc
{
    // Joint forward axis, in joint local-space, to be aimed at target position.
    Vector3        forward;
    // Offset position from the joint in local-space, that will aim at target.
    Vector3        offset;
    // Pole vector, in model-space. The pole vector defines the direction
    // the up should point to.
    Vector3        poleVector;
    // Array of joint indexes.
    const int32_t* jointChain;
    // Array of joint up axises, in joint local-space, used to keep the joint oriented in the
    // same direction as the pole vector.
    const Vector3* jointUpVectors;
    // Twist_angle rotates joint around the target vector.
    float          twistAngle;
    // Weight given to the IK correction clamped in range [0,1]. Applied to each joint in chain.
    float          jointWeight;
    // Chain Length
    int32_t        jointChainLength;
    // Optional boolean output value, set to true if target can be reached with IK
    // computations.
    bool           reached;
};

struct TwoBonesIKDesc
{
    // Pole vector, in model-space. The pole vector defines the direction the
    // middle joint should point to, allowing to control IK chain orientation.
    Vector3 poleVector;
    // Normalized middle joint rotation axis, in middle joint local-space.
    Vector3 midAxis;
    // Weight given to the IK correction clamped in range [0,1].
    float   weight;
    // Soften ratio allows the chain to gradually fall behind the target
    // position. This prevents the joint chain from snapping into the final
    // position, softening the final degrees before the joint chain becomes flat.
    // This ratio represents the distance to the end, from which softening is
    // starting.
    float   soften;
    // Twist_angle rotates IK chain around the vector define by start-to-target
    // vector.
    float   twistAngle;
    // Array of joint indexes.
    int32_t jointChain[3];
    // Optional boolean output value, set to true if target can be reached with IK
    // computations.
    bool    reached;
};

// Responsible for coordinating the posing of a Rig by an Animation
class FORGE_API AnimatedObject
{
public:
    // Set up an Animated object with the Rig it will be posing and the default animation to play when idle
    void Initialize(Rig* rig, Animation* animation);

    // Must be called to clean up the system if it has been initialized
    void Exit();

    // To be called every frame of the main application, handles sampling and updating the current animation
    bool Update(float dt);

    bool AimIK(AimIKDesc* params, const Point3& target);

    // Apply two bone inverse kinematic
    bool TwoBonesIK(TwoBonesIKDesc* params, const Point3& target);

    // Update mRigs world matricies
    void ComputePose(const Matrix4& rootTransform);

    // compute joint scales
    void ComputeJointScales(const Matrix4& rootTransform);

    // Have rig update to display in its bind pose
    void ComputeBindPose(const Matrix4& rootTransform);

    // Gets the world matrix without any scale data of the joint at index (returns identity if index is invalid)
    inline Matrix4 GetJointWorldMatNoScale(uint32_t index) const
    {
        ASSERT(rig && index < rig->numJoints);
        mat4 withScale = jointWorldMats[index];

        // Normalize the first three collumns
        vec4 col0 = vec4(normalize(withScale.getCol0().getXYZ()), withScale.getCol0().getW());
        vec4 col1 = vec4(normalize(withScale.getCol1().getXYZ()), withScale.getCol1().getW());
        vec4 col2 = vec4(normalize(withScale.getCol2().getXYZ()), withScale.getCol2().getW());

        mat4 withoutScale = mat4(col0, col1, col2, withScale.getCol3());
        return withoutScale;
    }

    // The Rig the AnimatedObject will be posing
    Rig* rig = NULL;

    // Pointer to the animation we are sampling
    Animation* animation = NULL;

    // Transform to apply to entire rig
    Matrix4 rootTransform = Matrix4::identity(); // TODO: Do we need this here? Might be better if it's provided in each call to
                                                 // ComputePose, less duplicated state

    // Buffer of local transforms as sampled from the animation.
    ozz::span<SoaTransform> localTrans;

    // Buffer of joint model space matrices set by animations
    ozz::span<Matrix4> jointModelMats;
    // Buffer of world model space matrices for joints.
    ozz::span<Matrix4> jointWorldMats;

    // Drawing the skeleton of animated objects
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    // Color of the joints
    Vector4 jointColor = vec4(.9f, .9f, .9f, 1.f); // white

    // Color of the bones
    Vector4 boneColor = vec4(0.9f, 0.6f, 0.1f, 1.0f); // orange

    // Scales to apply to each joint - will be proportional to length of its child's bone
    ozz::span<Vector3> jointScales;

#endif
};
