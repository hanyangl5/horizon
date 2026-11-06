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
    pRingBuffer->maxBufferSize = pBufferDesc->size;
    pRingBuffer->bufferAlignment = sizeof(float[4]);
    BufferLoadDesc loadDesc = {};
    loadDesc.desc = *pBufferDesc;
    loadDesc.desc.pName = "GPURingBuffer";
    loadDesc.ppBuffer = &pRingBuffer->pBuffer;
    addResource(&loadDesc, NULL);
}

void addUniformGPURingBuffer(Renderer* pRenderer, uint32_t requiredUniformBufferSize, GPURingBuffer* pRingBuffer, bool const ownMemory,
                             ResourceMemoryUsage memoryUsage)
{
    *pRingBuffer = {};
    pRingBuffer->pRenderer = pRenderer;

    const uint32_t uniformBufferAlignment = (uint32_t)pRenderer->pGpu->settings.uniformBufferAlignment;
    const uint32_t maxUniformBufferSize = requiredUniformBufferSize;
    pRingBuffer->bufferAlignment = uniformBufferAlignment;
    pRingBuffer->maxBufferSize = maxUniformBufferSize;

    BufferDesc ubDesc = {};
    ubDesc.descriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.memoryUsage = memoryUsage;
    ubDesc.flags =
        (ubDesc.memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY ? BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT : BUFFER_CREATION_FLAG_NONE) |
        BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;

    if (ownMemory)
        ubDesc.flags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    ubDesc.size = maxUniformBufferSize;
    ubDesc.pName = "UniformGPURingBuffer";
    BufferLoadDesc loadDesc = {};
    loadDesc.desc = ubDesc;
    loadDesc.ppBuffer = &pRingBuffer->pBuffer;
    addResource(&loadDesc, NULL);
}

void removeGPURingBuffer(GPURingBuffer* pRingBuffer) { removeResource(pRingBuffer->pBuffer); }

void resetGPURingBuffer(GPURingBuffer* pRingBuffer) { pRingBuffer->currentBufferOffset = 0; }

GPURingBufferOffset getGPURingBufferOffset(GPURingBuffer* pRingBuffer, uint32_t memoryRequirement, uint32_t alignment)
{
    uint32_t alignedSize = round_up(memoryRequirement, alignment ? alignment : pRingBuffer->bufferAlignment);

    if (alignedSize > pRingBuffer->maxBufferSize)
    {
        ASSERT(false && "Ring Buffer too small for memory requirement");
        return { NULL, 0 };
    }

    if (pRingBuffer->currentBufferOffset + alignedSize >= pRingBuffer->maxBufferSize)
    {
        pRingBuffer->currentBufferOffset = 0;
    }

    GPURingBufferOffset ret = { pRingBuffer->pBuffer, pRingBuffer->currentBufferOffset };
    pRingBuffer->currentBufferOffset += alignedSize;

    return ret;
}

void addGpuCmdRing(Renderer* pRenderer, const GpuCmdRingDesc* pDesc, GpuCmdRing* pOut)
{
    ASSERT(pDesc->poolCount <= MAX_GPU_CMD_POOLS_PER_RING);
    ASSERT(pDesc->cmdPerPoolCount <= MAX_GPU_CMDS_PER_POOL);

    pOut->poolCount = pDesc->poolCount;
    pOut->cmdPerPoolCount = pDesc->cmdPerPoolCount;

    CmdPoolDesc poolDesc = {};
    poolDesc.transient = false;
    poolDesc.pQueue = pDesc->pQueue;

    for (uint32_t pool = 0; pool < pDesc->poolCount; ++pool)
    {
        addCmdPool(pRenderer, &poolDesc, &pOut->pCmdPools[pool]);
        CmdDesc cmdDesc = {};
        cmdDesc.pPool = pOut->pCmdPools[pool];
        for (uint32_t cmd = 0; cmd < pDesc->cmdPerPoolCount; ++cmd)
        {
#ifdef ENABLE_GRAPHICS_DEBUG
            static char buffer[MAX_DEBUG_NAME_LENGTH];
            snprintf(buffer, sizeof(buffer), "GpuCmdRing Pool %u Cmd %u", pool, cmd);
            cmdDesc.pName = buffer;
#endif // ENABLE_GRAPHICS_DEBUG
            addCmd(pRenderer, &cmdDesc, &pOut->pCmds[pool][cmd]);

            if (pDesc->addSyncPrimitives)
            {
                addFence(pRenderer, &pOut->pFences[pool][cmd]);
                addSemaphore(pRenderer, &pOut->pSemaphores[pool][cmd]);
            }
        }
    }

    pOut->poolIndex = UINT32_MAX;
    pOut->cmdIndex = UINT32_MAX;
    pOut->fenceIndex = UINT32_MAX;
}

void removeGpuCmdRing(Renderer* pRenderer, GpuCmdRing* pRing)
{
    for (uint32_t pool = 0; pool < pRing->poolCount; ++pool)
    {
        for (uint32_t cmd = 0; cmd < pRing->cmdPerPoolCount; ++cmd)
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
        pRing->poolIndex = (pRing->poolIndex + 1) % pRing->poolCount;
        pRing->cmdIndex = 0;
        pRing->fenceIndex = 0;
    }

    if (pRing->cmdIndex + cmdCount > pRing->cmdPerPoolCount)
    {
        ASSERT(false && "Out of command buffers for this pool");
        return GpuCmdRingElement{};
    }

    GpuCmdRingElement ret = {};
    ret.pCmdPool = pRing->pCmdPools[pRing->poolIndex];
    ret.pCmds = &pRing->pCmds[pRing->poolIndex][pRing->cmdIndex];
    ret.pFence = pRing->pFences[pRing->poolIndex][pRing->fenceIndex];
    ret.pSemaphore = pRing->pSemaphores[pRing->poolIndex][pRing->fenceIndex];

    pRing->cmdIndex += cmdCount;
    ++pRing->fenceIndex;

    return ret;
}
