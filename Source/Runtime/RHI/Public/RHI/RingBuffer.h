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

#include <Core/IConfig.h>

#include <RHI/IGraphics.h>
#include <Core/ILog.h>

/************************************************************************/
/* RING BUFFER MANAGEMENT											  */
/************************************************************************/
typedef struct GPURingBuffer
{
    Renderer* pRenderer;
    Buffer*   pBuffer;

    uint32_t mBufferAlignment;
    uint64_t mMaxBufferSize;
    uint64_t mCurrentBufferOffset;
} GPURingBuffer;

typedef struct GPURingBufferOffset
{
    Buffer*  pBuffer;
    uint64_t mOffset;
} GPURingBufferOffset;

#ifndef MAX_GPU_CMD_POOLS_PER_RING
#define MAX_GPU_CMD_POOLS_PER_RING 64u
#endif
#ifndef MAX_GPU_CMDS_PER_POOL
#define MAX_GPU_CMDS_PER_POOL 4u
#endif

typedef struct GpuCmdRingDesc
{
    // Queue used to create the command pools
    Queue*   pQueue;
    // Number of command pools in this ring
    uint32_t mPoolCount;
    // Number of command buffers to be created per command pool
    uint32_t mCmdPerPoolCount;
    // Whether to add fence, semaphore for this ring
    bool     mAddSyncPrimitives;
} GpuCmdRingDesc;

typedef struct GpuCmdRingElement
{
    CmdPool*   pCmdPool;
    Cmd**      pCmds;
    Fence*     pFence;
    Semaphore* pSemaphore;
} GpuCmdRingElement;

// Lightweight wrapper that works as a ring for command pools, command buffers
typedef struct GpuCmdRing
{
    CmdPool*   pCmdPools[MAX_GPU_CMD_POOLS_PER_RING];
    Cmd*       pCmds[MAX_GPU_CMD_POOLS_PER_RING][MAX_GPU_CMDS_PER_POOL];
    Fence*     pFences[MAX_GPU_CMD_POOLS_PER_RING][MAX_GPU_CMDS_PER_POOL];
    Semaphore* pSemaphores[MAX_GPU_CMD_POOLS_PER_RING][MAX_GPU_CMDS_PER_POOL];
    uint32_t   mPoolIndex;
    uint32_t   mCmdIndex;
    uint32_t   mFenceIndex;
    uint32_t   mPoolCount;
    uint32_t   mCmdPerPoolCount;
} GpuCmdRing;

void addGPURingBuffer(Renderer* pRenderer, const BufferDesc* pBufferDesc, GPURingBuffer* pRingBuffer);

void addUniformGPURingBuffer(Renderer* pRenderer, uint32_t requiredUniformBufferSize, GPURingBuffer* pRingBuffer,
                             bool const ownMemory = false, ResourceMemoryUsage memoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU);
void removeGPURingBuffer(GPURingBuffer* pRingBuffer);

void resetGPURingBuffer(GPURingBuffer* pRingBuffer);

GPURingBufferOffset getGPURingBufferOffset(GPURingBuffer* pRingBuffer, uint32_t memoryRequirement, uint32_t alignment = 0);

void addGpuCmdRing(Renderer* pRenderer, const GpuCmdRingDesc* pDesc, GpuCmdRing* pOut);

void removeGpuCmdRing(Renderer* pRenderer, GpuCmdRing* pRing);

GpuCmdRingElement getNextGpuCmdRingElement(GpuCmdRing* pRing, bool cyclePool, uint32_t cmdCount);