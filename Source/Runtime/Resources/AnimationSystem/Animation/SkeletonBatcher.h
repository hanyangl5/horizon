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

#include "../../../Application/Interfaces/ICameraController.h"
#include "RHI/IGraphics.h"
#include "../../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "AnimatedObject.h"
#include "Rig.h"

enum JointMeshType
{
    QuadSphere,
    Cube
};
// Uniform data to send
struct UniformSkeletonBlock
{
    CameraMatrix projectView;
    mat4         viewMatrix;

    vec4  color[MAX_SKELETON_BATCHER_BLOCK_INSTANCES];
    // Point Light Information
    vec4  lightPosition;
    vec4  lightColor;
    vec4  jointColor;
    uint4 skeletonInfo;
    mat4  toWorldMat[MAX_SKELETON_BATCHER_BLOCK_INSTANCES];
};

// Description needed to handle buffer updates and draw calls
struct SkeletonRenderDesc
{
    Renderer* renderer;
    Buffer*   jointVertexBuffer;
    Buffer*   boneVertexBuffer;

    uint32_t frameCount;
    uint32_t maxSkeletonBatches;

    uint32_t jointVertexStride;
    uint32_t numJointPoints;

    uint32_t boneVertexStride;
    uint32_t numBonePoints;

    uint32_t maxAnimatedObjects;

    BufferCreationFlags creationFlag;
    bool                drawBones;
    JointMeshType       jointMeshType;

    const char* jointVertShaderName;
    const char* jointFragShaderName;
};

typedef struct SkeletonBatcherLoadDesc
{
    ReloadType  loadType;
    hz::Format  colorFormat;
    hz::Format  depthFormat;
    SampleCount sampleCount;
    uint32_t    sampleQuality;
} SkeletonBatcherLoadDesc;

// Allows for efficiently instance rendering all joints and bones of all skeletons in the scene
// Will eventually be a debug option and a part of a much larger Animation System's draw functionalities
class FORGE_API SkeletonBatcher
{
public:
    // Set up the pipeline and initialize the buffers
    void Initialize(const SkeletonRenderDesc& skeletonRenderDesc);

    // Must be called to clean up the object if initialize was called
    void Exit();

    void Load(const SkeletonBatcherLoadDesc* pDesc);

    void Unload(ReloadType reloadType);

    // Add a rig to the list of skeletons to draw
    void AddAnimatedObject(AnimatedObject* animatedObject);

    void RemoveAnimatedObject(AnimatedObject* animatedObject);
    void RemoveAllAnimatedObjects();

    void SetActiveRigs(uint32_t activeRigs);

    // Update uniforms that will be shared between all skeletons
    void SetSharedUniforms(const CameraMatrix& projViewMat, const mat4& viewMat, const Vector3& lightPos, const Vector3& lightColor);

    // Must be called on a single thread before any call to SetPerInstanceUniforms
    void PreSetInstanceUniforms(const uint32_t frameIndex);

    // Update all the instanced uniform data for each batch of joints and bones
    // Can be called asyncronously for different object ranges
    void SetPerInstanceUniforms(const uint32_t frameIndex, int32_t numObjects = -1, uint32_t objectsOffset = 0);

    // Instance draw all the skeletons
    void Draw(Cmd* cmd, const uint32_t frameIndex);

    Shader*        jointShader = NULL;
    Shader*        boneShader = NULL;
    Pipeline*      jointPipeline = NULL;
    Pipeline*      bonePipeline = NULL;
    RootSignature* rootSignature = NULL;

private:
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    uint32_t maxAnimatedObjects = 0;

    // List of Rigs whose skeletons need to be rendered
    AnimatedObject** animatedObjects = NULL;
    uint32_t*        cumulativeAnimatedObjectInstanceCount = NULL;

    uint32_t frameCount = 0;
    uint32_t maxSkeletonBatches = 0;
    uint32_t numAnimatedObjects = 0;
    uint32_t numActiveAnimatedObjects = 0;

    // Application variables used to be able to update buffers
    Renderer* renderer = NULL;
    Buffer*   jointVertexBuffer = NULL;
    Buffer*   boneVertexBuffer = NULL;
    uint32_t  jointVertexStride = 0;
    uint32_t  boneVertexStride = 0;
    uint32_t  numJointPoints = 0;
    uint32_t  numBonePoints = 0;

    uint32_t rootConstantIndex = 0;

    // Buffer pointers that will get updated for each batch to be rendered
    Buffer** projViewUniformBufferJoints = NULL;
    Buffer** projViewUniformBufferBones = NULL;

    // Uniform data for the joints and bones
    UniformSkeletonBlock* uniformDataJoints = NULL;

    const char* jointVertShaderName = NULL;
    const char* jointFragShaderName = NULL;

    tfrg_atomic32_t instanceCount = 0;

    // Keeps track of the number of batches we will send for instanced rendering
    // for each frame index
    tfrg_atomic32_t* batchCounts = NULL;

    // Keeps track of the size of the last batch as it can be less than MAX_INSTANCES
    tfrg_atomic32_t* batchSize = NULL;

    // Determines if this renderer will need to draw bones between each joint
    // Set in initialize
    bool drawBones = false;

    JointMeshType jointMeshType = QuadSphere;
#endif
};
