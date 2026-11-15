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

//--------------------------------------------------------------------------------------------
// NOTE: Make sure this is the last include in a .cpp file!
//       Never include this file from a header!!  If you must use the mem manager from a
//       header (which should be in rare cases, and usually only in core Forge source),
//       define "IMEMORY_FROM_HEADER" before including it.
//--------------------------------------------------------------------------------------------

#ifndef IMEMORY_H
#define IMEMORY_H

#include "Core/IConfig.h"
#ifdef __cplusplus
#include <new>
#include <utility> // std::forward only
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#define Kilobyte (1024)
#define Megabyte (1024 * Kilobyte)
#define Gigabyte (1024 * Megabyte)

typedef struct MemoryTrackingStats
{
    bool     TrackingEnabled;
    uint64_t LiveRequestedBytes;
    uint64_t LiveActualBytes;
    uint64_t LiveSlackBytes;
    uint64_t PeakRequestedBytes;
    uint64_t PeakActualBytes;
    uint64_t LiveAllocationCount;
    uint64_t PeakAllocationCount;
    uint64_t TotalAllocationCount;
    uint64_t ReallocationCount;
    uint64_t FailedAllocationCount;
    float    FragmentationPercent;
} MemoryTrackingStats;

#ifdef ENABLE_MEMORY_TRACKING
typedef struct MemoryStatistics
{
    uint32_t TotalReportedMemory;
    uint32_t TotalActualMemory;
    uint32_t PeakReportedMemory;
    uint32_t PeakActualMemory;
    uint32_t AccumulatedReportedMemory;
    uint32_t AccumulatedActualMemory;
    uint32_t AccumulatedAllocUnitCount;
    uint32_t TotalAllocUnitCount;
    uint32_t PeakAllocUnitCount;
} MemoryStatistics;
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    // appName is used to create dump file, pass NULL to avoid it
    FORGE_API bool                InitMemoryAlloc(const char* appName);
    FORGE_API void                ExitMemoryAlloc(void);
    FORGE_API MemoryTrackingStats GetMemoryTrackingStats(void);
    FORGE_API void                PlotMemoryTrackingStats(void);

#ifdef ENABLE_MEMORY_TRACKING
    FORGE_API MemoryStatistics GetMemoryStatistics(void);
#endif

    FORGE_API void* MemAllocateInternal(size_t Size, const char* f, int l, const char* sf);
    FORGE_API void* MemAllocateAlignedInternal(size_t align, size_t Size, const char* f, int l, const char* sf);
    FORGE_API void* MemCallocateInternal(size_t Count, size_t Size, const char* f, int l, const char* sf);
    FORGE_API void* MemCallocateAlignedInternal(size_t Count, size_t align, size_t Size, const char* f, int l, const char* sf);
    FORGE_API void* MemReallocateInternal(void* ptr, size_t Size, const char* f, int l, const char* sf);
    FORGE_API void  MemDeallocateInternal(void* ptr, const char* f, int l, const char* sf);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
template<typename T, typename... Args>
static T* MemPlacementNew(void* ptr, Args&&... args)
{
    return new (ptr) T(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
static T* MemNewInternal(const char* f, int l, const char* sf, Args&&... args)
{
    T* ptr = (T*)MemAllocateAlignedInternal(alignof(T), sizeof(T), f, l, sf);
    return MemPlacementNew<T>(ptr, std::forward<Args>(args)...);
}

template<typename T>
static void MemDeleteInternal(T* ptr, const char* f, int l, const char* sf)
{
    if (ptr)
    {
        ptr->~T();
        MemDeallocateInternal(ptr, f, l, sf);
    }
}
#endif

#ifndef MemAllocate
#define MemAllocate(Size) MemAllocateInternal(Size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef MemAllocateAligned
#define MemAllocateAligned(align, Size) MemAllocateAlignedInternal(align, Size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef MemCallocate
#define MemCallocate(Count, Size) MemCallocateInternal(Count, Size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef MemCallocateAligned
#define MemCallocateAligned(Count, align, Size) MemCallocateAlignedInternal(Count, align, Size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef MemReallocate
#define MemReallocate(ptr, Size) MemReallocateInternal(ptr, Size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef MemDeallocate
#define MemDeallocate(ptr) MemDeallocateInternal(ptr, __FILE__, __LINE__, __FUNCTION__)
#endif

#ifdef __cplusplus
#ifndef MemNew
#define MemNew(ObjectType, ...) MemNewInternal<ObjectType>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef MemDelete
#define MemDelete(ptr) MemDeleteInternal(ptr, __FILE__, __LINE__, __FUNCTION__)
#endif
#endif

#endif

// These guards only work per translation unit; unity builds concatenate them, so a macro set here
// would poison the next source file in the batch.
#if !defined(IMEMORY_FROM_HEADER) && !defined(HORIZON_UNITY_BUILD)
#ifndef malloc
#define malloc(Size) static_assert(false, "Please use MemAllocate");
#endif
#ifndef calloc
#define calloc(Count, Size) static_assert(false, "Please use MemCallocate");
#endif
#ifndef memalign
#define memalign(align, Size) static_assert(false, "Please use MemAllocateAligned");
#endif
#ifndef realloc
#define realloc(ptr, Size) static_assert(false, "Please use MemReallocate");
#endif
#ifndef free
#define free(ptr) static_assert(false, "Please use MemDeallocate");
#endif

#ifdef __cplusplus
#ifndef new
#define new static_assert(false, "Please use MemPlacementNew");
#endif
#ifndef delete
#define delete static_assert(false, "Please use MemDeallocate with explicit destructor call");
#endif
#endif

#endif

#ifdef IMEMORY_FROM_HEADER
#undef IMEMORY_FROM_HEADER
#endif
