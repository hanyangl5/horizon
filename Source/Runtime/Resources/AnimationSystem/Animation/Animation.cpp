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

#include "Animation.h"

void Animation::Initialize(AnimationDesc animationDesc)
{
    rig = animationDesc.rig;
    blendType = animationDesc.blendType;
    numClips = min(animationDesc.numLayers, MAX_NUM_CLIPS);

    numAdditiveClips = 0;

    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

    for (uint32_t i = 0; i < numClips; i++)
    {
        // Save clip structures
        clips[i] = animationDesc.layerProperties[i].clip;
        clipControllers[i] = animationDesc.layerProperties[i].clipController;
        clipMasks[i] = animationDesc.layerProperties[i].clipMask;

        // Save additive properties of additive clips
        clipControllers[i]->additive = animationDesc.layerProperties[i].additive;
        if (clipControllers[i]->additive)
            numAdditiveClips++;

        // Find the index of the longest clip
        if (duration < clipControllers[i]->duration)
        {
            duration = clipControllers[i]->duration;
            longestClipIndex = i;
        }

        // Prepare input and output of clip sampling

        // Allocates sampler runtime buffers.
        clipLocalTrans[i] = allocator->AllocateRange<SoaTransform>(rig->numSoaJoints);

        // Allocates a cache that matches animation requirements.
        clipSamplingCaches[i] = ozz::New<ozz::animation::SamplingJob::Context>(rig->numJoints);
    }

    // Allocate the blend layers that will be set each sampling based on each clip's properties
    layers = allocator->AllocateRange<ozz::animation::BlendingJob::Layer>(numClips - numAdditiveClips);
    additiveLayers = allocator->AllocateRange<ozz::animation::BlendingJob::Layer>(numAdditiveClips);
}

void Animation::Exit()
{
    ozz::memory::Allocator* allocator = ozz::memory::default_allocator();

    for (uint32_t i = 0; i < numClips; i++)
    {
        ozz::Delete(clipSamplingCaches[i]);
        allocator->Deallocate(clipLocalTrans[i]);
    }
    allocator->Deallocate(layers);
    allocator->Deallocate(additiveLayers);
}

bool Animation::Sample(float dt, ozz::span<SoaTransform>& localTrans)
{
    // update blend and sample parameters
    if (autoSetBlendParams)
    {
        UpdateBlendParameters();
    }

    // sample each of the clips that make up this animation
    for (uint32_t i = 0; i < numClips; i++)
    {
        // Updates clips time.
        clipControllers[i]->Update(dt);

        // Early out if this layers weight makes it irrelevant during blending.
        if (clipControllers[i]->weight != 0.f)
        {
            // if (!clips[i]->Sample(clipControllers[i]->timeRatio))
            if (!clips[i]->Sample(clipSamplingCaches[i], clipLocalTrans[i], clipControllers[i]->timeRatio))
                return false;
        }
    }

    // Update the animations current time ratio
    timeRatio = clipControllers[longestClipIndex]->timeRatio;

    // blend these samples together
    return Blend(localTrans);
}

void Animation::UpdateBlendParameters()
{
    // Set to Ozz's default min value to undo any external changes
    threshold = ozz::animation::BlendingJob().threshold;

    // Each animation will have equal influence
    if (blendType == BlendType::EQUAL)
    {
        for (uint32_t i = 0; i < numClips; i++)
        {
            clipControllers[i]->weight = 1.f / numClips;
        }
    }

    // The animations will fade into one another in the order they were added
    // Based on blendRatio
    else if (blendType == BlendType::CROSS_DISSOLVE)
    {
        // Computes weight parameters for all samplers.
        const float numIntervals = (float)numClips - 1;
        const float interval = 1.f / numIntervals;
        for (uint32_t i = 0; i < numClips; ++i)
        {
            const float med = i * interval; // unique order of animation between [0,1]
            const float x = blendRatio - med;
            const float y = ((x < 0.f ? x : -x) + interval) * numIntervals;

            clipControllers[i]->weight = max(0.f, y);
        }
    }

    // The animations will fade into one another in the order they were added, syncronizing their speeds as they fade into eachother
    // Based on blendRatio
    else if (blendType == BlendType::CROSS_DISSOLVE_SYNC)
    {
        // Computes weight parameters for all samplers.
        const float numIntervals = (float)numClips - 1;
        const float interval = 1.f / numIntervals;
        for (uint32_t i = 0; i < numClips; ++i)
        {
            const float med = i * interval; // unique order of animation between [0,1]
            const float x = blendRatio - med;
            const float y = ((x < 0.f ? x : -x) + interval) * numIntervals;

            clipControllers[i]->weight = max(0.f, y);
        }

        // Synchronizes animations.
        // First computes loop cycle duration. Selects the 2 Clips that define
        // interval that contains blendRatio.
        // Uses a maximum value smaller that 1.f (-epsilon) to ensure that
        // (relevantClip + 1) is always valid.
        const uint32_t relevantClip = (uint32_t)((blendRatio - 1e-3f) * (numClips - 1));
        ASSERT(relevantClip + 1 < numClips);
        ClipController* ClipControllerL = clipControllers[relevantClip];
        ClipController* ClipControllerR = clipControllers[relevantClip + 1];

        // Interpolates animation durations using their respective weights, to
        // find the loop cycle duration that matches blend_ratio_.
        const float loopDuration =

            ClipControllerL->duration * ClipControllerL->weight + ClipControllerR->duration * ClipControllerR->weight;

        // Finally finds the speed coefficient for all Clips.
        const float invLoopDuration = 1.f / loopDuration;
        for (uint32_t i = 0; i < numClips; ++i)
        {
            ClipController* ClipController = clipControllers[i];
            const float     speed = ClipController->duration * invLoopDuration;
            ClipController->playbackSpeed = speed;
        }
    }
}

bool Animation::Blend(ozz::span<SoaTransform>& localTrans)
{
    uint32_t additiveIndex = 0;
    for (uint32_t i = 0; i < numClips; i++)
    {
        if (clipControllers[i]->additive)
        {
            additiveLayers[additiveIndex].transform = clipLocalTrans[i];
            additiveLayers[additiveIndex].weight = clipControllers[i]->weight;

            if (clipMasks[i])
                additiveLayers[additiveIndex].joint_weights = clipMasks[i]->GetJointWeights();
            else
                additiveLayers[additiveIndex].joint_weights = ozz::span<const Vector4>();

            additiveIndex++;
        }
        else
        {
            layers[i].transform = clipLocalTrans[i];
            layers[i].weight = clipControllers[i]->weight;

            if (clipMasks[i])
                layers[i].joint_weights = clipMasks[i]->GetJointWeights();
            else
                layers[i].joint_weights = ozz::span<const Vector4>();
        }
    }

    // Setups blending job.
    ozz::animation::BlendingJob blendJob;
    blendJob.threshold = threshold;
    blendJob.layers = layers;
    if (numAdditiveClips > 0)
        blendJob.additive_layers = additiveLayers;
    blendJob.rest_pose = rig->skeleton.joint_rest_poses();
    blendJob.output = localTrans;

    // Blends.
    if (!blendJob.Run())
    {
        return false;
    }

    return true;
}

void Animation::SetTimeRatio(float timeRatio)
{
    float time = timeRatio * duration;

    for (uint32_t i = 0; i < numClips; i++)
    {
        clipControllers[i]->SetTimeRatio(time);
    }
}