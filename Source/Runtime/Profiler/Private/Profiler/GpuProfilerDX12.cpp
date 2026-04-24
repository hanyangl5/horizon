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

#include "GpuProfilerDX12.h"

#include "RHI/IGraphics.h"

#if defined(ENABLE_GPU_PROFILER) && !defined(DIRECT3D12)
#error "GpuProfilerDX12 requires the Direct3D12 RHI backend."
#endif

#if defined(ENABLE_GPU_PROFILER) && defined(DIRECT3D12) && defined(TRACY_ENABLE)

#include <tracy/TracyD3D12.hpp>

#include "Core/IMemory.h"

#include <string.h>

static TracyD3D12Ctx getTracyGpuContext(GpuProfiler* pGpuProfiler) { return (TracyD3D12Ctx)pGpuProfiler->pBackendGpuContext; }

static void freeTracyGpuZones(GpuProfiler* pGpuProfiler)
{
    if (!pGpuProfiler || !pGpuProfiler->pGpuTimerPool)
        return;

    for (uint32_t i = 0; i < pGpuProfiler->mCurrentPoolIndex; ++i)
    {
        GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[i];
        ASSERT(!pGpuTimer->mBackendGpuZoneActive);
        if (pGpuTimer->pBackendGpuZone)
        {
            tf_free(pGpuTimer->pBackendGpuZone);
            pGpuTimer->pBackendGpuZone = NULL;
        }
    }
}

void initGpuProfilerDX12(Renderer* pRenderer, Queue* pQueue, GpuProfiler* pGpuProfiler)
{
    pGpuProfiler->pBackendGpuContext = TracyD3D12Context(pRenderer->mDx.pDevice, pQueue->mDx.pQueue);
    if (pGpuProfiler->pBackendGpuContext)
    {
        TracyD3D12ContextName(getTracyGpuContext(pGpuProfiler), pGpuProfiler->mGroupName, (uint16_t)strlen(pGpuProfiler->mGroupName));
    }
}

void exitGpuProfilerDX12(GpuProfiler* pGpuProfiler)
{
    freeTracyGpuZones(pGpuProfiler);

    if (pGpuProfiler->pBackendGpuContext)
    {
        TracyD3D12Destroy(getTracyGpuContext(pGpuProfiler));
        pGpuProfiler->pBackendGpuContext = NULL;
    }
}

void cmdBeginGpuFrameProfileDX12(GpuProfiler* pGpuProfiler)
{
    if (pGpuProfiler->pBackendGpuContext)
    {
        TracyD3D12NewFrame(getTracyGpuContext(pGpuProfiler));
        TracyD3D12Collect(getTracyGpuContext(pGpuProfiler));
    }
}

void beginGpuTimestampQueryDX12(Cmd* pCmd, GpuProfiler* pGpuProfiler, GpuTimer* pGpuTimer)
{
    if (!pCmd || !pGpuProfiler || !pGpuTimer || !pGpuProfiler->pBackendGpuContext || pGpuTimer->mBackendGpuZoneActive)
        return;

    if (!pGpuTimer->pBackendGpuZone)
    {
        pGpuTimer->pBackendGpuZone = tf_malloc(sizeof(tracy::D3D12ZoneScope));
        ASSERT(pGpuTimer->pBackendGpuZone);
    }

    tf_placement_new<tracy::D3D12ZoneScope>((tracy::D3D12ZoneScope*)pGpuTimer->pBackendGpuZone, getTracyGpuContext(pGpuProfiler),
                                            (uint32_t)TracyLine, TracyFile, strlen(TracyFile), TracyFunction, strlen(TracyFunction),
                                            pGpuTimer->mName, strlen(pGpuTimer->mName), pCmd->mDx.pCmdList, true);
    pGpuTimer->mBackendGpuZoneActive = true;
}

void endGpuTimestampQueryDX12(GpuTimer* pGpuTimer)
{
    if (!pGpuTimer || !pGpuTimer->mBackendGpuZoneActive)
        return;

    ((tracy::D3D12ZoneScope*)pGpuTimer->pBackendGpuZone)->~D3D12ZoneScope();
    pGpuTimer->mBackendGpuZoneActive = false;
}

#else

void initGpuProfilerDX12(Renderer* pRenderer, Queue* pQueue, GpuProfiler* pGpuProfiler)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pQueue);
    UNREF_PARAM(pGpuProfiler);
}

void exitGpuProfilerDX12(GpuProfiler* pGpuProfiler) { UNREF_PARAM(pGpuProfiler); }

void cmdBeginGpuFrameProfileDX12(GpuProfiler* pGpuProfiler) { UNREF_PARAM(pGpuProfiler); }

void beginGpuTimestampQueryDX12(Cmd* pCmd, GpuProfiler* pGpuProfiler, GpuTimer* pGpuTimer)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pGpuProfiler);
    UNREF_PARAM(pGpuTimer);
}

void endGpuTimestampQueryDX12(GpuTimer* pGpuTimer) { UNREF_PARAM(pGpuTimer); }

#endif
