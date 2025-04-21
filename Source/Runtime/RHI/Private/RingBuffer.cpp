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

#include "Core/IConfig.h"
#include "RHI/IGraphics.h"

#include "Resources/IResourceLoader.h"
#include "Core/ILog.h"
#include "RHI/RingBuffer.h"

void addGPURingBuffer(Renderer* pRenderer, const BufferDesc* pBufferDesc, GPURingBuffer* pRingBuffer)
{
    *pRingBuffer = {};
    pRingBuffer->pRenderer = pRenderer;
    pRingBuffer->mMaxBufferSize = pBufferDesc->mSize;
    pRingBuffer->mBufferAlignment = sizeof(float[4]);
    BufferLoadDesc loadDesc = {};
    loadDesc.mDesc = *pBufferDesc;
    loadDesc.mDesc.pName = "GPURingBuffer";
    loadDesc.ppBuffer = &pRingBuffer->pBuffer;
    addResource(&loadDesc, NULL);
}

void addUniformGPURingBuffer(Renderer* pRenderer, uint32_t requiredUniformBufferSize, GPURingBuffer* pRingBuffer, bool const ownMemory,
                             ResourceMemoryUsage memoryUsage)
{
    *pRingBuffer = {};
    pRingBuffer->pRenderer = pRenderer;

    const uint32_t uniformBufferAlignment = (uint32_t)pRenderer->pGpu->mSettings.mUniformBufferAlignment;
    const uint32_t maxUniformBufferSize = requiredUniformBufferSize;
    pRingBuffer->mBufferAlignment = uniformBufferAlignment;
    pRingBuffer->mMaxBufferSize = maxUniformBufferSize;

    BufferDesc ubDesc = {};
    ubDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mMemoryUsage = memoryUsage;
    ubDesc.mFlags =
        (ubDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY ? BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT : BUFFER_CREATION_FLAG_NONE) |
        BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;

    if (ownMemory)
        ubDesc.mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    ubDesc.mSize = maxUniformBufferSize;
    ubDesc.pName = "UniformGPURingBuffer";
    BufferLoadDesc loadDesc = {};
    loadDesc.mDesc = ubDesc;
    loadDesc.ppBuffer = &pRingBuffer->pBuffer;
    addResource(&loadDesc, NULL);
}

void removeGPURingBuffer(GPURingBuffer* pRingBuffer) { removeResource(pRingBuffer->pBuffer); }

void resetGPURingBuffer(GPURingBuffer* pRingBuffer) { pRingBuffer->mCurrentBufferOffset = 0; }

GPURingBufferOffset getGPURingBufferOffset(GPURingBuffer* pRingBuffer, uint32_t memoryRequirement, uint32_t alignment)
{
    uint32_t alignedSize = round_up(memoryRequirement, alignment ? alignment : pRingBuffer->mBufferAlignment);

    if (alignedSize > pRingBuffer->mMaxBufferSize)
    {
        ASSERT(false && "Ring Buffer too small for memory requirement");
        return { NULL, 0 };
    }

    if (pRingBuffer->mCurrentBufferOffset + alignedSize >= pRingBuffer->mMaxBufferSize)
    {
        pRingBuffer->mCurrentBufferOffset = 0;
    }

    GPURingBufferOffset ret = { pRingBuffer->pBuffer, pRingBuffer->mCurrentBufferOffset };
    pRingBuffer->mCurrentBufferOffset += alignedSize;

    return ret;
}

void addGpuCmdRing(Renderer* pRenderer, const GpuCmdRingDesc* pDesc, GpuCmdRing* pOut)
{
    ASSERT(pDesc->mPoolCount <= MAX_GPU_CMD_POOLS_PER_RING);
    ASSERT(pDesc->mCmdPerPoolCount <= MAX_GPU_CMDS_PER_POOL);

    pOut->mPoolCount = pDesc->mPoolCount;
    pOut->mCmdPerPoolCount = pDesc->mCmdPerPoolCount;

    CmdPoolDesc poolDesc = {};
    poolDesc.mTransient = false;
    poolDesc.pQueue = pDesc->pQueue;

    for (uint32_t pool = 0; pool < pDesc->mPoolCount; ++pool)
    {
        addCmdPool(pRenderer, &poolDesc, &pOut->pCmdPools[pool]);
        CmdDesc cmdDesc = {};
        cmdDesc.pPool = pOut->pCmdPools[pool];
        for (uint32_t cmd = 0; cmd < pDesc->mCmdPerPoolCount; ++cmd)
        {
#ifdef ENABLE_GRAPHICS_DEBUG
            static char buffer[MAX_DEBUG_NAME_LENGTH];
            snprintf(buffer, sizeof(buffer), "GpuCmdRing Pool %u Cmd %u", pool, cmd);
            cmdDesc.pName = buffer;
#endif // ENABLE_GRAPHICS_DEBUG
            addCmd(pRenderer, &cmdDesc, &pOut->pCmds[pool][cmd]);

            if (pDesc->mAddSyncPrimitives)
            {
                addFence(pRenderer, &pOut->pFences[pool][cmd]);
                addSemaphore(pRenderer, &pOut->pSemaphores[pool][cmd]);
            }
        }
    }

    pOut->mPoolIndex = UINT32_MAX;
    pOut->mCmdIndex = UINT32_MAX;
    pOut->mFenceIndex = UINT32_MAX;
}

void removeGpuCmdRing(Renderer* pRenderer, GpuCmdRing* pRing)
{
    for (uint32_t pool = 0; pool < pRing->mPoolCount; ++pool)
    {
        for (uint32_t cmd = 0; cmd < pRing->mCmdPerPoolCount; ++cmd)
        {
            removeCmd(pRenderer, pRing->pCmds[pool][cmd]);
            if (pRing->pSemaphores[pool][cmd])
            {
                removeSemaphore(pRenderer, pRing->pSemaphores[pool][cmd]);
            }
            if (pRing->pFences[pool][cmd])
            {
                removeFence(pRenderer, pRing->pFences[pool][cmd]);
            }
        }
        removeCmdPool(pRenderer, pRing->pCmdPools[pool]);
    }
    *pRing = {};
}

GpuCmdRingElement getNextGpuCmdRingElement(GpuCmdRing* pRing, bool cyclePool, uint32_t cmdCount)
{
    if (cyclePool)
    {
        pRing->mPoolIndex = (pRing->mPoolIndex + 1) % pRing->mPoolCount;
        pRing->mCmdIndex = 0;
        pRing->mFenceIndex = 0;
    }

    if (pRing->mCmdIndex + cmdCount > pRing->mCmdPerPoolCount)
    {
        ASSERT(false && "Out of command buffers for this pool");
        return GpuCmdRingElement{};
    }

    GpuCmdRingElement ret = {};
    ret.pCmdPool = pRing->pCmdPools[pRing->mPoolIndex];
    ret.pCmds = &pRing->pCmds[pRing->mPoolIndex][pRing->mCmdIndex];
    ret.pFence = pRing->pFences[pRing->mPoolIndex][pRing->mFenceIndex];
    ret.pSemaphore = pRing->pSemaphores[pRing->mPoolIndex][pRing->mFenceIndex];

    pRing->mCmdIndex += cmdCount;
    ++pRing->mFenceIndex;

    return ret;
}
