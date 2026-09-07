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

#include "Profiler/IProfiler.h"

#if defined(ENABLE_PROFILER)
#include "Core/IMemory.h"

#include <stdio.h>
#include <string.h>
#include <tracy/TracyC.h>
#include <tracy/Tracy.hpp>
#endif

#if defined(ENABLE_GPU_PROFILER)
void initGpuProfilers();
void exitGpuProfilers();
#endif

#if defined(ENABLE_PROFILER)
struct TracyCpuProfileToken
{
    ___tracy_source_location_data mSourceLocation;
    char                          mName[256];
};

static uint64_t packTracyZoneCtx(TracyCZoneCtx ctx)
{
    static_assert(sizeof(ctx) <= sizeof(uint64_t), "TracyCZoneCtx must fit in the legacy profiler tick storage.");
    uint64_t packed = 0;
    memcpy(&packed, &ctx, sizeof(ctx));
    return packed;
}

static TracyCZoneCtx unpackTracyZoneCtx(uint64_t packed)
{
    TracyCZoneCtx ctx = {};
    memcpy(&ctx, &packed, sizeof(ctx));
    return ctx;
}

static void formatTracyCpuProfileName(char* pBuffer, size_t bufferSize, const char* pGroup, const char* pName)
{
    if (pGroup && pGroup[0] && pName && pName[0])
    {
        snprintf(pBuffer, bufferSize, "%s/%s", pGroup, pName);
    }
    else
    {
        snprintf(pBuffer, bufferSize, "%s", pName && pName[0] ? pName : (pGroup && pGroup[0] ? pGroup : "CPU"));
    }
}

#if defined(ENABLE_TRACY_MEMORY)
#if defined(DIRECT3D12)
extern void d3d12_plotMemoryStats(Renderer* pRenderer);
#endif

static Renderer* gMemoryPlotRenderers[8] = {};
static uint32_t  gMemoryPlotRendererCount = 0;

static void registerMemoryPlotRenderer(Renderer* pRenderer)
{
    if (!pRenderer)
        return;

    for (uint32_t i = 0; i < gMemoryPlotRendererCount; ++i)
    {
        if (gMemoryPlotRenderers[i] == pRenderer)
            return;
    }

    if (gMemoryPlotRendererCount < TF_ARRAY_COUNT(gMemoryPlotRenderers))
    {
        gMemoryPlotRenderers[gMemoryPlotRendererCount++] = pRenderer;
    }
}
#endif
#endif

void initProfiler(ProfilerDesc* pDesc)
{
#if defined(ENABLE_TRACY_MEMORY)
    if (pDesc)
    {
        registerMemoryPlotRenderer(pDesc->pRenderer);
    }
#endif

#if defined(ENABLE_GPU_PROFILER)
    initGpuProfilers();

    if (pDesc && pDesc->mGpuProfilerCount > 0)
    {
        ASSERT(pDesc->pRenderer != NULL && pDesc->ppQueues != NULL && pDesc->ppProfilerNames != NULL && pDesc->pProfileTokens != NULL);
        if (!pDesc->pRenderer || !pDesc->ppQueues || !pDesc->ppProfilerNames || !pDesc->pProfileTokens)
            return;

        for (uint32_t i = 0; i < pDesc->mGpuProfilerCount; ++i)
        {
            pDesc->pProfileTokens[i] = addGpuProfiler(pDesc->pRenderer, pDesc->ppQueues[i], pDesc->ppProfilerNames[i]);
        }
    }
#else
    UNREF_PARAM(pDesc);
#endif
}

void exitProfiler()
{
#if defined(ENABLE_GPU_PROFILER)
    exitGpuProfilers();
#endif
#if defined(ENABLE_TRACY_MEMORY)
    memset(gMemoryPlotRenderers, 0, sizeof(gMemoryPlotRenderers));
    gMemoryPlotRendererCount = 0;
#endif
}

void flipProfiler()
{
#if defined(ENABLE_PROFILER)
    FrameMark;
#if defined(ENABLE_TRACY_MEMORY)
    memPlotTrackingStats();
#if defined(DIRECT3D12)
    for (uint32_t i = 0; i < gMemoryPlotRendererCount; ++i)
    {
        d3d12_plotMemoryStats(gMemoryPlotRenderers[i]);
    }
#endif
#endif
#endif
}

// Legacy MicroProfile benchmark/dump APIs are parked while Tracy capture export is wired up.
// void setAggregateFrames(uint32_t nFrames) { UNREF_PARAM(nFrames); }
// void dumpProfileData(const char* appName, uint32_t nMaxFrames)
// {
//     UNREF_PARAM(appName);
//     UNREF_PARAM(nMaxFrames);
// }
// void dumpBenchmarkData(IApp::Settings* pSettings, const char* outFilename, const char* appName)
// {
//     UNREF_PARAM(pSettings);
//     UNREF_PARAM(outFilename);
//     UNREF_PARAM(appName);
// }

float2 cmdDrawGpuProfile(Cmd* pCmd, float2 screenCoordsInPx, ProfileToken nProfileToken, FontDrawDesc* pDrawDesc)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(nProfileToken);
    UNREF_PARAM(pDrawDesc);
    return screenCoordsInPx;
}

float2 cmdDrawCpuProfile(Cmd* pCmd, float2 screenCoordsInPx, FontDrawDesc* pDrawDesc)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pDrawDesc);
    return screenCoordsInPx;
}

void toggleProfilerUI(bool active) { UNREF_PARAM(active); }

void toggleProfilerMenuUI(bool active) { UNREF_PARAM(active); }

void toggleProfilerDrawing(bool active) { UNREF_PARAM(active); }

uint64_t cpuProfileEnter(ProfileToken nToken)
{
#if defined(ENABLE_PROFILER)
    if (nToken == PROFILE_INVALID_TOKEN)
        return 0;

    const TracyCpuProfileToken* pToken = (const TracyCpuProfileToken*)nToken;
    return packTracyZoneCtx(___tracy_emit_zone_begin_callstack(&pToken->mSourceLocation, TRACY_CALLSTACK, true));
#else
    UNREF_PARAM(nToken);
    return 0;
#endif
}

void cpuProfileLeave(ProfileToken nToken, uint64_t nTick)
{
#if defined(ENABLE_PROFILER)
    UNREF_PARAM(nToken);
    ___tracy_emit_zone_end(unpackTracyZoneCtx(nTick));
#else
    UNREF_PARAM(nToken);
    UNREF_PARAM(nTick);
#endif
}

ProfileToken getCpuProfileToken(const char* pGroup, const char* pName, uint32_t nColor)
{
#if defined(ENABLE_PROFILER)
    TracyCpuProfileToken* pToken = (TracyCpuProfileToken*)tf_malloc(sizeof(*pToken));
    ASSERT(pToken);
    if (!pToken)
        return PROFILE_INVALID_TOKEN;

    formatTracyCpuProfileName(pToken->mName, sizeof(pToken->mName), pGroup, pName);

    pToken->mSourceLocation.name = pToken->mName;
    pToken->mSourceLocation.function = "cpuProfileEnter";
    pToken->mSourceLocation.file = "Profiler/IProfiler.h";
    pToken->mSourceLocation.line = 0;
    pToken->mSourceLocation.color = nColor;
    return (ProfileToken)pToken;
#else
    UNREF_PARAM(pGroup);
    UNREF_PARAM(pName);
    UNREF_PARAM(nColor);
    return PROFILE_INVALID_TOKEN;
#endif
}

void removeCpuProfileToken(ProfileToken nToken)
{
#if defined(ENABLE_PROFILER)
    if (nToken == PROFILE_INVALID_TOKEN)
        return;

    tf_free((void*)nToken);
#else
    UNREF_PARAM(nToken);
#endif
}

uint64_t cpuProfileEnterNamed(const char* pGroup, const char* pName, uint32_t nColor)
{
#if defined(ENABLE_PROFILER)
    char name[256];
    formatTracyCpuProfileName(name, sizeof(name), pGroup, pName);

    const char* function = "cpuProfileEnterNamed";
    const char* file = "Profiler/IProfiler.h";
    uint64_t    sourceLocation = ___tracy_alloc_srcloc_name(0, file, strlen(file), function, strlen(function), name, strlen(name), nColor);
    return packTracyZoneCtx(___tracy_emit_zone_begin_alloc_callstack(sourceLocation, TRACY_CALLSTACK, true));
#else
    UNREF_PARAM(pGroup);
    UNREF_PARAM(pName);
    UNREF_PARAM(nColor);
    return 0;
#endif
}

float getCpuProfileTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    UNREF_PARAM(pGroup);
    UNREF_PARAM(pName);
    UNREF_PARAM(pThreadID);
    return -1.0f;
}

float getCpuProfileAvgTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    UNREF_PARAM(pGroup);
    UNREF_PARAM(pName);
    UNREF_PARAM(pThreadID);
    return -1.0f;
}

float getCpuProfileMinTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    UNREF_PARAM(pGroup);
    UNREF_PARAM(pName);
    UNREF_PARAM(pThreadID);
    return -1.0f;
}

float getCpuProfileMaxTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    UNREF_PARAM(pGroup);
    UNREF_PARAM(pName);
    UNREF_PARAM(pThreadID);
    return -1.0f;
}

float getCpuFrameTime() { return -1.0f; }

float getCpuAvgFrameTime() { return -1.0f; }

float getCpuMinFrameTime() { return -1.0f; }

float getCpuMaxFrameTime() { return -1.0f; }
