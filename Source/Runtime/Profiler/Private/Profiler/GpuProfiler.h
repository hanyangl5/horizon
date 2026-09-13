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
#include "Core/IMath.h"

struct Cmd;
struct Renderer;
struct Buffer;
struct Queue;
struct QueryPool;
typedef uint64_t ProfileToken;

typedef struct GpuTimer
{
    static const uint32_t LENGTH_OF_HISTORY = 60;

    char     name[64] = "Timer";
    uint32_t index = 0;
    uint32_t historyIndex = 0;
    uint32_t depth = 0;

    uint64_t     startGpuTime = 0;
    uint64_t     endGpuTime = 0;
    uint64_t     gpuTime = 0;
    uint64_t     gpuMinTime = 0;
    uint64_t     gpuMaxTime = 0;
    uint64_t     gpuHistory[LENGTH_OF_HISTORY] = {};
    size_t       hash = 0;
    ProfileToken token = {};
    void*        pBackendGpuZone = NULL;
    GpuTimer*    pParent = NULL;
    bool         debugMarker = false;
    bool         started = false;
    bool         backendGpuZoneActive = false;

} GpuTimer;

typedef struct GpuProfiler
{
    // double buffered
    static const uint32_t NUM_OF_FRAMES = 3;
    static const uint32_t MAX_TIMERS = 512;

    Renderer*  pRenderer = {};
    QueryPool* pQueryPool[NUM_OF_FRAMES] = {};
    uint32_t   currentTimerCount[NUM_OF_FRAMES] = {};
    double     gpuTimeStampFrequency = 0.0;

    uint32_t profilerIndex = 0;
    uint32_t bufferIndex = 0;
    uint32_t currentPoolIndex = 0;

    GpuTimer* pGpuTimerPool = NULL;
    GpuTimer* pCurrentNode = NULL;
    void*     pBackendGpuContext = NULL;

    char groupName[256] = "GPU";
} GpuProfiler;

struct GpuProfilerContainer
{
    static const uint32_t MAX_GPU_PROFILERS = 8;
    GpuProfiler*          profilers[MAX_GPU_PROFILERS] = { NULL };
    uint32_t              size = 0;
};
