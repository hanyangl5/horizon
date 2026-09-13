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

#include "GpuProfiler.h"

#include "Profiler/IProfiler.h"

#include "RHI/IGraphics.h"

#ifndef ENABLE_GPU_PROFILER
ProfileToken addGpuProfiler(Renderer* pRenderer, Queue* pQueue, const char* pName) { return PROFILE_INVALID_TOKEN; }
void         cmdBeginGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken, bool bUseMarker) {}
void         cmdEndGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken) {}
ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken, const char* pName, bool bUseMarker)
{
    return PROFILE_INVALID_TOKEN;
}
void         cmdEndGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken) {}
float        getGpuProfileTime(ProfileToken nProfileToken) { return -1.0f; }
float        getGpuProfileAvgTime(ProfileToken nProfileToken) { return -1.0f; }
float        getGpuProfileMinTime(ProfileToken nProfileToken) { return -1.0f; }
float        getGpuProfileMaxTime(ProfileToken nProfileToken) { return -1.0f; }
uint64_t     getGpuProfileTicksPerSecond(ProfileToken nProfileToken) { return 0; }
GpuProfiler* getGpuProfiler(ProfileToken nProfileToken) { return NULL; }
void         removeGpuProfiler(ProfileToken nProfileToken) {}
#else

#include "GpuProfilerBackend.h"

#include "RHI/IGraphics.h"
#include "../../../RHI/Private/RendererResourceAPI.h"
#include "Resources/IResourceLoader.h"
#include "Core/ILog.h"
#include "Core/ITime.h"

#include <string.h>

#include "Core/IMemory.h"

GpuProfilerContainer* gGpuProfilerContainer = NULL;

inline ProfileToken getProfileToken(uint32_t nProfilerIndex, uint32_t nTimerIndex)
{
    return ((uint64_t)nProfilerIndex << 32) | nTimerIndex;
}

inline uint32_t getProfileIndex(ProfileToken nToken) { return nToken >> 32; }

inline uint32_t getTimerIndex(ProfileToken nToken) { return nToken & 0xffff; }

GpuProfiler* getGpuProfiler(ProfileToken nProfileToken)
{
    if (nProfileToken == PROFILE_INVALID_TOKEN)
        return NULL;

    return gGpuProfilerContainer->profilers[getProfileIndex(nProfileToken)];
}

static void calculateTimes(Cmd* pCmd, GpuProfiler* pGpuProfiler, uint32_t index)
{
    GpuTimer* pRoot = &pGpuProfiler->pGpuTimerPool[index];
    if (!pRoot || !pRoot->started)
        return;

    uint64_t       elapsedTime = 0;
    const uint32_t historyIndex = pRoot->historyIndex;

    const uint32_t id = pRoot->index;
    QueryData      timestamp = {};
    getQueryData(pGpuProfiler->pRenderer, pGpuProfiler->pQueryPool[pGpuProfiler->bufferIndex], id, &timestamp);
    const uint64_t timestamp1 = timestamp.beginTimestamp;
    const uint64_t timestamp2 = timestamp.endTimestamp;

    elapsedTime = timestamp2 - timestamp1;
    if (timestamp2 <= timestamp1)
    {
        elapsedTime = 0;
    }
    else
    {
        pRoot->startGpuTime = timestamp1;
        pRoot->endGpuTime = timestamp2;
        pRoot->gpuTime = elapsedTime;
        pRoot->gpuMinTime = min(pRoot->gpuMinTime, elapsedTime);
        pRoot->gpuMaxTime = max(pRoot->gpuMaxTime, elapsedTime);
    }
    pRoot->gpuHistory[historyIndex] = elapsedTime;

    pRoot->historyIndex = (historyIndex + 1) % GpuTimer::LENGTH_OF_HISTORY;

    for (uint32_t i = index + 1; i < pGpuProfiler->currentPoolIndex; ++i)
    {
        if (pGpuProfiler->pGpuTimerPool[i].pParent == pRoot)
        {
            calculateTimes(pCmd, pGpuProfiler, i);
        }
    }

    pRoot->started = false; // Reset
}

double getAverageGpuTime(struct GpuProfiler* pGpuProfiler, struct GpuTimer* pGpuTimer)
{
    uint64_t elapsedTime = 0;

    for (uint32_t i = 0; i < GpuTimer::LENGTH_OF_HISTORY; ++i)
    {
        elapsedTime += pGpuTimer->gpuHistory[i];
    }

    return ((double)(elapsedTime / GpuTimer::LENGTH_OF_HISTORY) / pGpuProfiler->gpuTimeStampFrequency) * 1000.0;
}

void addGpuProfiler(Renderer* pRenderer, Queue* pQueue, GpuProfiler** ppGpuProfiler, const char* pName)
{
    GpuProfiler* pGpuProfiler = (GpuProfiler*)tf_calloc(1, sizeof(*pGpuProfiler));
    ASSERT(pGpuProfiler);

    tf_placement_new<GpuProfiler>(pGpuProfiler);
    pGpuProfiler->pRenderer = pRenderer;
    strncpy(pGpuProfiler->groupName, pName, sizeof pGpuProfiler->groupName - 1);
    pGpuProfiler->groupName[sizeof pGpuProfiler->groupName - 1] = 0;

    QueryPoolDesc queryHeapDesc = {};
    queryHeapDesc.queryCount = GpuProfiler::MAX_TIMERS;
    queryHeapDesc.type = QUERY_TYPE_TIMESTAMP;

    for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
    {
        addQueryPool(pRenderer, &queryHeapDesc, &pGpuProfiler->pQueryPool[i]);
    }

    getTimestampFrequency(pQueue, &pGpuProfiler->gpuTimeStampFrequency);

    initGpuProfilerBackend(pRenderer, pQueue, pGpuProfiler);

    pGpuProfiler->pGpuTimerPool = (GpuTimer*)tf_calloc(GpuProfiler::MAX_TIMERS, sizeof(*pGpuProfiler->pGpuTimerPool));
    pGpuProfiler->pCurrentNode = &pGpuProfiler->pGpuTimerPool[0];
    pGpuProfiler->currentPoolIndex = 0;

    *ppGpuProfiler = pGpuProfiler;
}

void removeGpuProfiler(struct GpuProfiler* pGpuProfiler)
{
    for (uint32_t i = 0; i < GpuProfiler::NUM_OF_FRAMES; ++i)
    {
        removeQueryPool(pGpuProfiler->pRenderer, pGpuProfiler->pQueryPool[i]);
    }

    exitGpuProfilerBackend(pGpuProfiler);

    tf_free(pGpuProfiler->pGpuTimerPool);
    tf_free(pGpuProfiler);
}

ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, const char* pName, bool addMarker = true,
                                       const float3& color = { 1, 1, 0 }, bool isRoot = false)
{
    GpuTimer* node = NULL;
    size_t    nameHash = tf_mem_hash<char>(pName, strlen(pName));

    for (GpuTimer* parent = pGpuProfiler->pCurrentNode; parent; parent = parent->pParent)
    {
        nameHash = tf_mem_hash<char>(parent->name, strlen(parent->name), nameHash);
    }

    for (uint32_t i = 0; i < pGpuProfiler->currentPoolIndex; ++i)
    {
        GpuTimer* tNode = &pGpuProfiler->pGpuTimerPool[i];
        if (nameHash == tNode->hash)
        {
            node = tNode;
            break;
        }
    }

    if (!node)
    {
        // first time seeing this
        ASSERT(pGpuProfiler->currentPoolIndex < GpuProfiler::MAX_TIMERS && "Profiler exceeded MAX_TIMERS.");
        node = &pGpuProfiler->pGpuTimerPool[pGpuProfiler->currentPoolIndex];
        strncpy(node->name, pName, sizeof node->name - 1);
        node->name[sizeof node->name - 1] = 0;
        node->hash = nameHash;
        node->historyIndex = 0;
        node->gpuMaxTime = 0;
        node->gpuMinTime = (uint64_t)-1;
        node->startGpuTime = isRoot ? 0 : pGpuProfiler->pCurrentNode->startGpuTime;
        node->endGpuTime = 0;
        node->token = getProfileToken(pGpuProfiler->profilerIndex, pGpuProfiler->currentPoolIndex);
        memset(node->gpuHistory, 0, sizeof(node->gpuHistory));

        ++pGpuProfiler->currentPoolIndex;
    }

    node->index = pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex];
    node->pParent = isRoot ? NULL : pGpuProfiler->pCurrentNode;
    node->depth = isRoot ? 0 : node->pParent->depth + 1; //-V522
    node->started = true;
    node->debugMarker = addMarker;

    if (!isRoot)
    {
        pGpuProfiler->pCurrentNode = node;
    }

    QueryDesc desc = { node->index };
    cmdBeginQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->bufferIndex], &desc);

    if (addMarker)
    {
        cmdBeginDebugMarker(pCmd, color.getX(), color.getY(), color.getZ(), pName);
    }

    beginGpuTimestampQueryBackend(pCmd, pGpuProfiler, node);

    ASSERT(pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex] < pGpuProfiler->currentPoolIndex &&
           "Duplicate timers found in one gpu frame");
    ++pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex];
    return node->token;
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, struct GpuProfiler* pGpuProfiler, bool isRoot = false)
{
    UNREF_PARAM(isRoot);

    endGpuTimestampQueryBackend(pGpuProfiler->pCurrentNode);

    QueryDesc desc = { pGpuProfiler->pCurrentNode->index };
    cmdEndQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->bufferIndex], &desc);

    if (pGpuProfiler->pCurrentNode->debugMarker)
    {
        cmdEndDebugMarker(pCmd);
    }

    pGpuProfiler->pCurrentNode = pGpuProfiler->pCurrentNode->pParent;
}

void initGpuProfilers()
{
    gGpuProfilerContainer = (GpuProfilerContainer*)tf_calloc(1, sizeof(*gGpuProfilerContainer));
    ASSERT(gGpuProfilerContainer);
    tf_placement_new<GpuProfilerContainer>(gGpuProfilerContainer);
}

void exitGpuProfilers()
{
    if (gGpuProfilerContainer == NULL)
        return;

    for (uint32_t i = 0; i < GpuProfilerContainer::MAX_GPU_PROFILERS; ++i)
    {
        if (gGpuProfilerContainer->profilers[i])
        {
            removeGpuProfiler(gGpuProfilerContainer->profilers[i]);
            gGpuProfilerContainer->profilers[i] = NULL;
        }
    }

    tf_free(gGpuProfilerContainer);
    gGpuProfilerContainer = NULL;
}

ProfileToken addGpuProfiler(Renderer* pRenderer, Queue* pQueue, const char* pName)
{
    if (!pRenderer->pGpu->settings.timestampQueries)
    {
        LOGF(LogLevel::eWARNING, "GPU timestamp queries not supported");
        return PROFILE_INVALID_TOKEN;
    }

    if (gGpuProfilerContainer->size >= GpuProfilerContainer::MAX_GPU_PROFILERS)
    {
        LOGF(LogLevel::eWARNING, "Reached maximum amount of Gpu Profilers");
        return PROFILE_INVALID_TOKEN;
    }

    GpuProfiler* pGpuProfiler;
    addGpuProfiler(pRenderer, pQueue, &pGpuProfiler, pName);
    ASSERT(pGpuProfiler);

    for (uint32_t i = 0; i < GpuProfilerContainer::MAX_GPU_PROFILERS; ++i)
    {
        if (!gGpuProfilerContainer->profilers[i])
        {
            gGpuProfilerContainer->profilers[i] = pGpuProfiler;
            ++gGpuProfilerContainer->size;
            pGpuProfiler->profilerIndex = i;
            break;
        }
    }

    return getProfileToken(pGpuProfiler->profilerIndex, 0);
}

void removeGpuProfiler(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    removeGpuProfiler(pGpuProfiler);
    gGpuProfilerContainer->profilers[getProfileIndex(nProfileToken)] = NULL;
    --gGpuProfilerContainer->size;
}

void cmdBeginGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken, bool bUseMarker)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    uint32_t nextIndex = (pGpuProfiler->bufferIndex + 1) % GpuProfiler::NUM_OF_FRAMES;
    pGpuProfiler->bufferIndex = nextIndex;

    cmdBeginGpuFrameProfileBackend(pGpuProfiler);

    calculateTimes(pCmd, pGpuProfiler, 0);

    if (pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex])
    {
        cmdResetQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->bufferIndex], 0,
                      pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex]);
    }

    pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex] = 0;

    cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, pGpuProfiler->groupName, bUseMarker, { 1, 1, 0 }, true);
    pGpuProfiler->pCurrentNode = &pGpuProfiler->pGpuTimerPool[0];
}

void cmdEndGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    cmdEndGpuTimestampQuery(pCmd, pGpuProfiler, true);

    cmdResolveQuery(pCmd, pGpuProfiler->pQueryPool[pGpuProfiler->bufferIndex], 0,
                    pGpuProfiler->currentTimerCount[pGpuProfiler->bufferIndex]);
}

ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken, const char* pName, bool bUseMarker)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return PROFILE_INVALID_TOKEN;

    return cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, pName, bUseMarker);
}

void cmdEndGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return;

    cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
}

float getGpuProfileTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)(pGpuTimer->gpuTime / pGpuProfiler->gpuTimeStampFrequency) * 1000.0f;
}

float getGpuProfileAvgTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)getAverageGpuTime(pGpuProfiler, pGpuTimer);
}

float getGpuProfileMinTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)(pGpuTimer->gpuMinTime / pGpuProfiler->gpuTimeStampFrequency) * 1000.0f;
}

float getGpuProfileMaxTime(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return -1.0f;

    GpuTimer* pGpuTimer = &pGpuProfiler->pGpuTimerPool[getTimerIndex(nProfileToken)];
    if (!pGpuTimer)
        return -1.0f;

    return (float)(pGpuTimer->gpuMaxTime / pGpuProfiler->gpuTimeStampFrequency) * 1000.0f;
}

uint64_t getGpuProfileTicksPerSecond(ProfileToken nProfileToken)
{
    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler)
        return 0;

    return (uint64_t)pGpuProfiler->gpuTimeStampFrequency;
}
#endif
