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

#include "SkeletonBatcher.h"

#include <ThirdParty/stb/stb_ds.h>

void SkeletonBatcher::Initialize(const SkeletonRenderDesc& skeletonRenderDesc)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // Set member render variables based on the description
    renderer = skeletonRenderDesc.renderer;
    jointVertexBuffer = skeletonRenderDesc.jointVertexBuffer;
    numJointPoints = skeletonRenderDesc.numJointPoints;
    jointVertexStride = skeletonRenderDesc.jointVertexStride;
    boneVertexStride = skeletonRenderDesc.boneVertexStride;
    jointMeshType = skeletonRenderDesc.jointMeshType;
    frameCount = skeletonRenderDesc.frameCount;
    maxSkeletonBatches = skeletonRenderDesc.maxSkeletonBatches;
    jointVertShaderName = skeletonRenderDesc.jointVertShaderName;
    jointFragShaderName = skeletonRenderDesc.jointFragShaderName;

    ASSERT(frameCount > 0);
    ASSERT(maxSkeletonBatches > 0);

    ASSERT(skeletonRenderDesc.maxAnimatedObjects > 0 && "Need to specify the maximum number of animated objects");
    maxAnimatedObjects = skeletonRenderDesc.maxAnimatedObjects;
    arrsetlen(animatedObjects, skeletonRenderDesc.maxAnimatedObjects);
    arrsetlen(cumulativeAnimatedObjectInstanceCount, skeletonRenderDesc.maxAnimatedObjects + 1);
    memset(animatedObjects, 0, sizeof(*animatedObjects) * skeletonRenderDesc.maxAnimatedObjects);
    memset(cumulativeAnimatedObjectInstanceCount, 0,
           sizeof(*cumulativeAnimatedObjectInstanceCount) * (skeletonRenderDesc.maxAnimatedObjects + 1));

    // Determine if we will ever expect to use this renderer to draw bones
    drawBones = skeletonRenderDesc.drawBones;
    if (drawBones)
    {
        boneVertexBuffer = skeletonRenderDesc.boneVertexBuffer;
        numBonePoints = skeletonRenderDesc.numBonePoints;
    }

    numAnimatedObjects = 0;
    numActiveAnimatedObjects = 0;
    instanceCount = 0;

    projViewUniformBufferJoints = (Buffer**)tf_calloc_memalign(frameCount * maxSkeletonBatches * 2, alignof(Buffer*), sizeof(Buffer*));
    projViewUniformBufferBones = projViewUniformBufferJoints + (frameCount * maxSkeletonBatches);
    uniformDataJoints =
        (UniformSkeletonBlock*)tf_calloc_memalign(maxSkeletonBatches, alignof(UniformSkeletonBlock), sizeof(UniformSkeletonBlock));

    batchCounts = (tfrg_atomic32_t*)tf_calloc_memalign(frameCount + frameCount * maxSkeletonBatches, alignof(tfrg_atomic32_t),
                                                       sizeof(tfrg_atomic32_t));
    batchSize = batchCounts + frameCount;

    // Initialize all the buffer that will be used for each batch per each frame index
    BufferLoadDesc ubDesc = {};
    ubDesc.desc.descriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.desc.memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.desc.size = sizeof(UniformSkeletonBlock);
    ubDesc.desc.flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | skeletonRenderDesc.creationFlag;
    ubDesc.pData = NULL;

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        for (uint32_t j = 0; j < maxSkeletonBatches; ++j)
        {
            const uint32_t bufferIndex = i * maxSkeletonBatches + j;

            ubDesc.ppBuffer = &projViewUniformBufferJoints[bufferIndex];
            addResource(&ubDesc, NULL);

            if (drawBones)
            {
                ubDesc.ppBuffer = &projViewUniformBufferBones[bufferIndex];
                addResource(&ubDesc, NULL);
            }
        }
    }

#endif
}

void SkeletonBatcher::Exit()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        for (uint32_t j = 0; j < maxSkeletonBatches; ++j)
        {
            const uint32_t bufferIndex = i * maxSkeletonBatches + j;
            removeResource(projViewUniformBufferJoints[bufferIndex]);
            if (drawBones)
            {
                removeResource(projViewUniformBufferBones[bufferIndex]);
            }
        }
    }

    arrfree(animatedObjects);
    arrfree(cumulativeAnimatedObjectInstanceCount);

    tf_free((void*)batchCounts);
    tf_free(uniformDataJoints);
    tf_free(projViewUniformBufferJoints);
#endif
}

void SkeletonBatcher::Load(const SkeletonBatcherLoadDesc* pDesc)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    if (pDesc->loadType & RELOAD_TYPE_SHADER)
    {
        ShaderLoadDesc jointShader = {};
        // if a custom shader is required for joints
        if (jointVertShaderName && jointFragShaderName)
        {
            jointShader.stages[0].pFileName = jointVertShaderName;
            jointShader.stages[1].pFileName = jointFragShaderName;
        }
        else
        {
            jointShader.stages[0].pFileName = "joint.vert";
            jointShader.stages[1].pFileName = "joint.frag";
        }

        ShaderLoadDesc boneShader = {};
        boneShader.stages[0].pFileName = "bone.vert";
        boneShader.stages[1].pFileName = "bone.frag";

        addShader(renderer, &jointShader, &this->jointShader);
        addShader(renderer, &boneShader, &this->boneShader);

        Shader*           shaders[] = { this->jointShader, this->boneShader };
        RootSignatureDesc rootDesc = {};
        rootDesc.shaderCount = 2;
        rootDesc.ppShaders = shaders;

        addRootSignature(renderer, &rootDesc, &rootSignature);

        rootConstantIndex = getDescriptorIndexFromName(rootSignature, "RootConstant0");
    }

    if (pDesc->loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        VertexLayout jointLayout = {};
        jointLayout.bindingCount = 1;
        jointLayout.attribCount = 2;
        jointLayout.attribs[0].semantic = SEMANTIC_POSITION;
        jointLayout.attribs[0].format = hz::Format::R32G32B32_SFLOAT;
        jointLayout.attribs[0].binding = 0;
        jointLayout.attribs[0].location = 0;
        jointLayout.attribs[0].offset = 0;
        jointLayout.attribs[1].semantic = SEMANTIC_NORMAL;
        jointLayout.attribs[1].format = hz::Format::R32G32B32_SFLOAT;
        jointLayout.attribs[1].binding = 0;
        jointLayout.attribs[1].location = 1;
        jointLayout.attribs[1].offset = 3 * sizeof(float);

        VertexLayout boneVertexLayout = {};
        boneVertexLayout.bindingCount = 1;
        boneVertexLayout.attribCount = 3;
        boneVertexLayout.attribs[0].semantic = SEMANTIC_POSITION;
        boneVertexLayout.attribs[0].format = hz::Format::R32G32B32_SFLOAT;
        boneVertexLayout.attribs[0].binding = 0;
        boneVertexLayout.attribs[0].location = 0;
        boneVertexLayout.attribs[0].offset = 0;
        boneVertexLayout.attribs[1].semantic = SEMANTIC_NORMAL;
        boneVertexLayout.attribs[1].format = hz::Format::R32G32B32_SFLOAT;
        boneVertexLayout.attribs[1].binding = 0;
        boneVertexLayout.attribs[1].location = 1;
        boneVertexLayout.attribs[1].offset = 3 * sizeof(float);
        boneVertexLayout.attribs[2].semantic = SEMANTIC_JOINTS;
        boneVertexLayout.attribs[2].format = hz::Format::R16G16B16A16_UINT;
        boneVertexLayout.attribs[2].binding = 0;
        boneVertexLayout.attribs[2].location = 2;
        boneVertexLayout.attribs[2].offset = 6 * sizeof(float);

        RasterizerStateDesc skeletonRasterizerStateDesc = {};
        skeletonRasterizerStateDesc.cullMode = CULL_MODE_FRONT;

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.depthTest = true;
        depthStateDesc.depthWrite = true;
        depthStateDesc.depthFunc = CMP_GEQUAL;

        PipelineDesc desc = {};
        desc.type = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.graphicsDesc;
        if (jointMeshType == Cube)
        {
            pipelineSettings.primitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        }
        else
        {
            pipelineSettings.primitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
        }
        pipelineSettings.renderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = (hz::Format*)&pDesc->colorFormat;
        pipelineSettings.sampleCount = pDesc->sampleCount;
        pipelineSettings.sampleQuality = pDesc->sampleQuality;
        pipelineSettings.depthStencilFormat = pDesc->depthFormat;
        pipelineSettings.pRootSignature = rootSignature;

        pipelineSettings.pShaderProgram = jointShader;
        pipelineSettings.pVertexLayout = &jointLayout;
        pipelineSettings.pRasterizerState = &skeletonRasterizerStateDesc;
        addPipeline(renderer, &desc, &jointPipeline);

        pipelineSettings.primitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.pShaderProgram = boneShader;
        pipelineSettings.pVertexLayout = &boneVertexLayout;
        addPipeline(renderer, &desc, &bonePipeline);
    }
#endif
}
void SkeletonBatcher::Unload(ReloadType reloadType)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG

    if (reloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        removePipeline(renderer, bonePipeline);
        removePipeline(renderer, jointPipeline);
    }

    if (reloadType & RELOAD_TYPE_SHADER)
    {
        removeRootSignature(renderer, rootSignature);
        removeShader(renderer, boneShader);
        removeShader(renderer, jointShader);
    }
#endif
}

void SkeletonBatcher::SetSharedUniforms(const CameraMatrix& projViewMat, const mat4& viewMat, const Vector3& lightPos,
                                        const Vector3& lightColor)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // we only need the rotation of view matrix in shaders to calculate billboards and fake lighting
    mat4 viewMatNoTranslation = viewMat;
    viewMatNoTranslation.setCol(3, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    for (uint32_t i = 0; i < maxSkeletonBatches; ++i)
    {
        uniformDataJoints[i].viewMatrix = viewMatNoTranslation;
        uniformDataJoints[i].projectView = projViewMat;
        uniformDataJoints[i].lightPosition = Vector4(lightPos);
        uniformDataJoints[i].lightColor = Vector4(lightColor);
    }
#endif
}

void SkeletonBatcher::SetActiveRigs(uint32_t activeRigs)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    numActiveAnimatedObjects = min(activeRigs, numAnimatedObjects);
#endif
}

void SkeletonBatcher::PreSetInstanceUniforms(const uint32_t frameIndex)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // Reset batch counts
    tfrg_atomic32_t* pFrameBatchSize = &batchSize[frameIndex * maxSkeletonBatches];

    tfrg_atomic32_store_relaxed(&batchCounts[frameIndex], 0);
    for (uint32_t i = 0; i < maxSkeletonBatches; ++i)
    {
        tfrg_atomic32_store_relaxed(&pFrameBatchSize[i], 0);
    }
#endif
}

void SkeletonBatcher::SetPerInstanceUniforms(const uint32_t frameIndex, int32_t numObjects, uint32_t objectsOffset)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    ASSERT(frameIndex < frameCount);

    // If the numObjects parameter was not initialized, used the data from all the active rigs
    if (numObjects == -1)
    {
        numObjects = numActiveAnimatedObjects;
    }

    const uint32_t lastBatchIndex = cumulativeAnimatedObjectInstanceCount[numActiveAnimatedObjects] / MAX_SKELETON_BATCHER_BLOCK_INSTANCES;
    const uint32_t lastBatchSize = cumulativeAnimatedObjectInstanceCount[numActiveAnimatedObjects] % MAX_SKELETON_BATCHER_BLOCK_INSTANCES;

    // Will keep track of the number of instances that have their data added
    const uint32_t totalInstanceCount =
        cumulativeAnimatedObjectInstanceCount[objectsOffset + numObjects] - cumulativeAnimatedObjectInstanceCount[objectsOffset];
    uint32_t instanceCount = tfrg_atomic32_add_relaxed(&this->instanceCount, totalInstanceCount);

    // Last resets instanceCount
    if (instanceCount + totalInstanceCount == cumulativeAnimatedObjectInstanceCount[numActiveAnimatedObjects])
        this->instanceCount = 0;

    uint32_t batchInstanceCount = 0;
    uint32_t batchIndex = instanceCount / MAX_SKELETON_BATCHER_BLOCK_INSTANCES;

    tfrg_atomic32_t* pFrameBatchSize = &batchSize[frameIndex * maxSkeletonBatches];

    // For every rig
    for (uint32_t objIndex = objectsOffset; objIndex < numObjects + objectsOffset; ++objIndex)
    {
        const AnimatedObject* animObj = animatedObjects[objIndex];

        // Get the number of joints in the rig
        uint32_t numJoints = animObj->rig->numJoints;
        // For every joint in the rig
        for (uint32_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
        {
            uint32_t              instanceIndex = instanceCount % MAX_SKELETON_BATCHER_BLOCK_INSTANCES;
            UniformSkeletonBlock& uniformDataJoints = this->uniformDataJoints[batchIndex];

            this->uniformDataJoints[batchIndex].skeletonInfo = uint4(numJoints, 1, 0, 0);
            uniformDataJoints.toWorldMat[instanceIndex] =
                animObj->GetJointWorldMatNoScale(jointIndex) * mat4::scale(animObj->jointScales[jointIndex]);
            uniformDataJoints.color[instanceIndex] = animObj->boneColor;
            uniformDataJoints.jointColor = animObj->jointColor;

            // increment the count of uniform data that has been filled for this batch
            ++instanceCount;
            ++batchInstanceCount;

            // If we have reached our maximun amount of instances, or the end of our data
            if ((instanceIndex == MAX_SKELETON_BATCHER_BLOCK_INSTANCES - 1) ||
                ((objIndex - objectsOffset == (uint32_t)(numObjects - 1)) && (jointIndex == numJoints - 1)))
            {
                // Finalize the data for this batch by adding the batch instance to the batch total size
                uint32_t currBatchSize = tfrg_atomic32_add_relaxed(&pFrameBatchSize[batchIndex], batchInstanceCount) + batchInstanceCount;

                // Only update if batch is full, or this is the last batch as it could be less than MAX_SKELETON_BATCHER_BLOCK_INSTANCES
                if (currBatchSize == MAX_SKELETON_BATCHER_BLOCK_INSTANCES ||
                    (lastBatchIndex == batchIndex && currBatchSize == lastBatchSize))
                {
                    const uint32_t bufferIndex = frameIndex * maxSkeletonBatches + batchIndex;

                    tfrg_atomic32_add_relaxed(&batchCounts[frameIndex], 1);
                    BufferUpdateDesc viewProjCbvJoints = { projViewUniformBufferJoints[bufferIndex] };
                    beginUpdateResource(&viewProjCbvJoints);
                    memcpy(viewProjCbvJoints.pMappedData, &uniformDataJoints, sizeof(uniformDataJoints));
                    endUpdateResource(&viewProjCbvJoints);
                }

                // Increase batchIndex for next batch
                ++batchIndex;
                // Reset the count so it can be used for the next batch
                batchInstanceCount = 0;
            }
        }
    }
#endif
}

void SkeletonBatcher::AddAnimatedObject(AnimatedObject* animatedObject)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    ASSERT(animatedObject && animatedObject->rig);
    for (uint32_t i = 0; i < numAnimatedObjects; ++i)
    {
        ASSERT(animatedObject != animatedObjects[i] && "Trying to add duplicated animated object");
    }

    // Adds the rig so its data can be used and increments the rig count
    ASSERT(numAnimatedObjects < (uint32_t)arrlen(animatedObjects) && "Exceed maximum amount of rigs");
    animatedObjects[numAnimatedObjects] = animatedObject;
    uint32_t joints = animatedObject->rig->numJoints + cumulativeAnimatedObjectInstanceCount[numAnimatedObjects];
    ASSERT(joints < MAX_SKELETON_BATCHER_BLOCK_INSTANCES * maxSkeletonBatches && "Exceed maximum amount of instances");
    ++numAnimatedObjects;
    ++numActiveAnimatedObjects;
    cumulativeAnimatedObjectInstanceCount[numAnimatedObjects] = joints;
#endif
}

void SkeletonBatcher::RemoveAnimatedObject(AnimatedObject* animatedObject)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    ASSERT(animatedObject);

    uint32_t start = numAnimatedObjects;
    for (uint32_t i = 0; i < numAnimatedObjects; ++i)
    {
        if (animatedObject == animatedObjects[i])
        {
            start = i;
            break;
        }
    }

    if (start < numAnimatedObjects)
    {
        ASSERT(animatedObjects[start] == animatedObject);
        for (uint32_t i = start; i < numAnimatedObjects - 1; ++i)
        {
            ASSERT(animatedObjects[i + 1] != animatedObject && "Animated object was added twice");
            animatedObjects[i] = animatedObjects[i + 1];

            ASSERT(animatedObjects[i]->rig);

            const uint32_t cumulativeJoints = animatedObjects[i]->rig->numJoints + cumulativeAnimatedObjectInstanceCount[i];
            cumulativeAnimatedObjectInstanceCount[i + 1] = cumulativeJoints;
            ASSERT(cumulativeJoints < MAX_SKELETON_BATCHER_BLOCK_INSTANCES * maxSkeletonBatches && "Exceed maximum amount of instances");
        }

        numAnimatedObjects--;
        if (numActiveAnimatedObjects > start)
            numActiveAnimatedObjects--;

        ASSERT(numActiveAnimatedObjects <= numAnimatedObjects);
    }

#endif
}

void SkeletonBatcher::RemoveAllAnimatedObjects()
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    memset(animatedObjects, 0, sizeof(AnimatedObject*) * numAnimatedObjects);
    memset(cumulativeAnimatedObjectInstanceCount, 0, sizeof(cumulativeAnimatedObjectInstanceCount[0]) * numAnimatedObjects);
    numAnimatedObjects = 0;
    numActiveAnimatedObjects = 0;
#endif
}

void SkeletonBatcher::Draw(Cmd* cmd, const uint32_t frameIndex)
{
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
    // Get the number of batches to draw for this frameindex
    uint32_t numBatches = tfrg_atomic32_store_relaxed(&batchCounts[frameIndex], 0);
    if (numBatches == 0)
    {
        return;
    }

    tfrg_atomic32_t* pFrameBatchSize = &batchSize[frameIndex * maxSkeletonBatches];

    cmdBindPipeline(cmd, jointPipeline);

    // Joints
    cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Joints");
    cmdBindVertexBuffer(cmd, 1, &jointVertexBuffer, &jointVertexStride, NULL);

    // for each batch of joints
    for (uint32_t batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        const uint32_t uniformIndex = getBufferCbvIndex(projViewUniformBufferJoints[frameIndex * maxSkeletonBatches + batchIndex]);
        cmdBindPushConstants(cmd, rootSignature, rootConstantIndex, &uniformIndex);
        cmdDrawInstanced(cmd, numJointPoints / 6, 0, pFrameBatchSize[batchIndex], 0);
        if (!drawBones)
            pFrameBatchSize[batchIndex] = 0;
    }
    cmdEndDebugMarker(cmd);

    // Bones
    if (drawBones)
    {
        cmdBindPipeline(cmd, bonePipeline);
        cmdBindVertexBuffer(cmd, 1, &boneVertexBuffer, &boneVertexStride, NULL);
        cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons Bones");

        // for each batch of bones

        const AnimatedObject* animObj = animatedObjects[0];
        uint32_t              numJoints = animObj->rig->numJoints;

        for (uint32_t batchIndex = 0; batchIndex < numBatches; batchIndex++)
        {
            uint32_t instanceCount = pFrameBatchSize[batchIndex] / numJoints;
            const uint32_t uniformIndex = getBufferCbvIndex(projViewUniformBufferBones[frameIndex * maxSkeletonBatches + batchIndex]);
            cmdBindPushConstants(cmd, rootSignature, rootConstantIndex, &uniformIndex);
            cmdDrawInstanced(cmd, numBonePoints / 8, 0, instanceCount, 0);

            pFrameBatchSize[batchIndex] = 0;
        }
        cmdEndDebugMarker(cmd);
    }
#endif
}
