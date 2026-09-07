
#include "Core/IConfig.h"

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

#include "wchar.h"

#include <stdint.h>
#include <memory.h>
#include <stdlib.h>

#if defined(ENABLE_TRACY_MEMORY)
#if defined(_MSC_VER)
#include <intrin.h>
#include <malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif
#include <tracy/TracyC.h>
#endif

//#include "../ThirdParty/OpenSource/ModifiedSonyMath/vectormath_settings.hpp"
//#include "Core/IMath.h"
#define MEM_MAX(a, b)             ((a) > (b) ? (a) : (b))

#define ALIGN_TO(size, alignment) (((size) + (alignment)-1) & ~((alignment)-1))

// Taken from EASTL EA_PLATFORM_MIN_MALLOC_ALIGNMENT
#ifndef PLATFORM_MIN_MALLOC_ALIGNMENT
#if defined(__APPLE__)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 16
#elif defined(__ANDROID__) && defined(ARCH_ARM_FAMILY)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 8
#elif defined(NX64) && defined(ARCH_ARM_FAMILY)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 8
#elif defined(__ANDROID__) && defined(ARCH_X86_FAMILY)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 8
#else
#define PLATFORM_MIN_MALLOC_ALIGNMENT (PTR_SIZE * 2)
#endif
#endif

//#define MIN_ALLOC_ALIGNMENT MEM_MAX(VECTORMATH_MIN_ALIGN, PLATFORM_MIN_MALLOC_ALIGNMENT)

#define MIN_ALLOC_ALIGNMENT MEM_MAX(16, PLATFORM_MIN_MALLOC_ALIGNMENT)

#ifndef HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH
#define HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH 16
#endif

static bool tf_mul_size(size_t lhs, size_t rhs, size_t* result)
{
    if (lhs != 0 && rhs > SIZE_MAX / lhs)
        return false;

    *result = lhs * rhs;
    return true;
}

#if defined(ENABLE_TRACY_MEMORY)

#define TF_MEMORY_TRACY_POOL "CPU/tf"

typedef enum MemoryTrackingEntryState
{
    MEMORY_TRACKING_ENTRY_EMPTY = 0,
    MEMORY_TRACKING_ENTRY_OCCUPIED = 1,
    MEMORY_TRACKING_ENTRY_TOMBSTONE = 2,
} MemoryTrackingEntryState;

typedef struct MemoryTrackingEntry
{
    void*   pPtr;
    size_t  mRequestedSize;
    size_t  mActualSize;
    size_t  mAlignment;
    uint8_t mState;
} MemoryTrackingEntry;

static MemoryTrackingEntry* gMemoryTrackingEntries = NULL;
static size_t               gMemoryTrackingCapacity = 0;
static size_t               gMemoryTrackingCount = 0;
static size_t               gMemoryTrackingTombstoneCount = 0;
static MemoryTrackingStats  gMemoryTrackingStats = { 0 };

#if defined(_MSC_VER)
static volatile long gMemoryTrackingLock = 0;

static void memoryTrackingLock(void)
{
    while (_InterlockedCompareExchange(&gMemoryTrackingLock, 1, 0) != 0)
    {
        _mm_pause();
    }
}

static void memoryTrackingUnlock(void) { _InterlockedExchange(&gMemoryTrackingLock, 0); }
#else
static volatile int gMemoryTrackingLock = 0;

static void memoryTrackingLock(void)
{
    while (__sync_lock_test_and_set(&gMemoryTrackingLock, 1) != 0)
    {
    }
}

static void memoryTrackingUnlock(void) { __sync_lock_release(&gMemoryTrackingLock); }
#endif

static size_t memoryTrackingHashPtr(const void* ptr)
{
    uintptr_t hash = (uintptr_t)ptr;
    hash >>= 4;
#if UINTPTR_MAX > 0xFFFFFFFFu
    hash ^= hash >> 33;
    hash *= (uintptr_t)0xff51afd7ed558ccdull;
    hash ^= hash >> 33;
    hash *= (uintptr_t)0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33;
#else
    hash ^= hash >> 16;
    hash *= (uintptr_t)0x7feb352du;
    hash ^= hash >> 15;
    hash *= (uintptr_t)0x846ca68bu;
    hash ^= hash >> 16;
#endif
    return (size_t)hash;
}

static void memoryTrackingApplyAllocLocked(size_t requestedSize, size_t actualSize)
{
    uint64_t requested = (uint64_t)requestedSize;
    uint64_t actual = (uint64_t)actualSize;

    if (actual < requested)
        actual = requested;

    gMemoryTrackingStats.mLiveRequestedBytes += requested;
    gMemoryTrackingStats.mLiveActualBytes += actual;
    ++gMemoryTrackingStats.mLiveAllocationCount;
    ++gMemoryTrackingStats.mTotalAllocationCount;

    if (gMemoryTrackingStats.mLiveRequestedBytes > gMemoryTrackingStats.mPeakRequestedBytes)
        gMemoryTrackingStats.mPeakRequestedBytes = gMemoryTrackingStats.mLiveRequestedBytes;
    if (gMemoryTrackingStats.mLiveActualBytes > gMemoryTrackingStats.mPeakActualBytes)
        gMemoryTrackingStats.mPeakActualBytes = gMemoryTrackingStats.mLiveActualBytes;
    if (gMemoryTrackingStats.mLiveAllocationCount > gMemoryTrackingStats.mPeakAllocationCount)
        gMemoryTrackingStats.mPeakAllocationCount = gMemoryTrackingStats.mLiveAllocationCount;
}

static void memoryTrackingApplyFreeLocked(size_t requestedSize, size_t actualSize)
{
    uint64_t requested = (uint64_t)requestedSize;
    uint64_t actual = (uint64_t)actualSize;

    if (actual < requested)
        actual = requested;

    if (gMemoryTrackingStats.mLiveRequestedBytes >= requested)
        gMemoryTrackingStats.mLiveRequestedBytes -= requested;
    else
        gMemoryTrackingStats.mLiveRequestedBytes = 0;

    if (gMemoryTrackingStats.mLiveActualBytes >= actual)
        gMemoryTrackingStats.mLiveActualBytes -= actual;
    else
        gMemoryTrackingStats.mLiveActualBytes = 0;

    if (gMemoryTrackingStats.mLiveAllocationCount > 0)
        --gMemoryTrackingStats.mLiveAllocationCount;
}

static MemoryTrackingEntry* memoryTrackingFindEntryLocked(void* ptr)
{
    if (!ptr || !gMemoryTrackingEntries || gMemoryTrackingCapacity == 0)
        return NULL;

    size_t index = memoryTrackingHashPtr(ptr) & (gMemoryTrackingCapacity - 1);
    for (size_t probe = 0; probe < gMemoryTrackingCapacity; ++probe)
    {
        MemoryTrackingEntry* entry = &gMemoryTrackingEntries[index];
        if (entry->mState == MEMORY_TRACKING_ENTRY_EMPTY)
            return NULL;
        if (entry->mState == MEMORY_TRACKING_ENTRY_OCCUPIED && entry->pPtr == ptr)
            return entry;

        index = (index + 1) & (gMemoryTrackingCapacity - 1);
    }

    return NULL;
}

static bool memoryTrackingReserveLocked(size_t minCapacity)
{
    size_t newCapacity = gMemoryTrackingCapacity ? gMemoryTrackingCapacity : 1024;
    while (newCapacity < minCapacity)
        newCapacity *= 2;

    MemoryTrackingEntry* newEntries = (MemoryTrackingEntry*)calloc(newCapacity, sizeof(MemoryTrackingEntry));
    if (!newEntries)
        return false;

    MemoryTrackingEntry* oldEntries = gMemoryTrackingEntries;
    size_t               oldCapacity = gMemoryTrackingCapacity;

    gMemoryTrackingEntries = newEntries;
    gMemoryTrackingCapacity = newCapacity;
    gMemoryTrackingCount = 0;
    gMemoryTrackingTombstoneCount = 0;

    for (size_t i = 0; i < oldCapacity; ++i)
    {
        MemoryTrackingEntry* oldEntry = &oldEntries[i];
        if (oldEntry->mState != MEMORY_TRACKING_ENTRY_OCCUPIED)
            continue;

        size_t index = memoryTrackingHashPtr(oldEntry->pPtr) & (gMemoryTrackingCapacity - 1);
        while (gMemoryTrackingEntries[index].mState == MEMORY_TRACKING_ENTRY_OCCUPIED)
            index = (index + 1) & (gMemoryTrackingCapacity - 1);

        gMemoryTrackingEntries[index] = *oldEntry;
        ++gMemoryTrackingCount;
    }

    free(oldEntries);
    return true;
}

static bool memoryTrackingEnsureInsertCapacityLocked(void)
{
    if (!gMemoryTrackingEntries)
        return memoryTrackingReserveLocked(1024);

    if ((gMemoryTrackingCount + gMemoryTrackingTombstoneCount + 1) * 10 >= gMemoryTrackingCapacity * 7)
        return memoryTrackingReserveLocked((gMemoryTrackingCount + 1) * 10 >= gMemoryTrackingCapacity * 7 ? gMemoryTrackingCapacity * 2
                                                                                                          : gMemoryTrackingCapacity);

    return true;
}

static bool memoryTrackingInsertLocked(void* ptr, size_t requestedSize, size_t actualSize, size_t alignment, bool* replaced)
{
    if (replaced)
        *replaced = false;

    if (!memoryTrackingEnsureInsertCapacityLocked())
        return false;

    MemoryTrackingEntry* existing = memoryTrackingFindEntryLocked(ptr);
    if (existing)
    {
        if (replaced)
            *replaced = true;
        memoryTrackingApplyFreeLocked(existing->mRequestedSize, existing->mActualSize);
        existing->mRequestedSize = requestedSize;
        existing->mActualSize = actualSize;
        existing->mAlignment = alignment;
        memoryTrackingApplyAllocLocked(requestedSize, actualSize);
        return true;
    }

    size_t               index = memoryTrackingHashPtr(ptr) & (gMemoryTrackingCapacity - 1);
    MemoryTrackingEntry* tombstone = NULL;

    for (;;)
    {
        MemoryTrackingEntry* entry = &gMemoryTrackingEntries[index];
        if (entry->mState == MEMORY_TRACKING_ENTRY_EMPTY)
        {
            if (tombstone)
                entry = tombstone;

            entry->pPtr = ptr;
            entry->mRequestedSize = requestedSize;
            entry->mActualSize = actualSize;
            entry->mAlignment = alignment;
            entry->mState = MEMORY_TRACKING_ENTRY_OCCUPIED;
            ++gMemoryTrackingCount;
            if (entry == tombstone && gMemoryTrackingTombstoneCount > 0)
                --gMemoryTrackingTombstoneCount;
            memoryTrackingApplyAllocLocked(requestedSize, actualSize);
            return true;
        }
        if (entry->mState == MEMORY_TRACKING_ENTRY_TOMBSTONE && !tombstone)
        {
            tombstone = entry;
        }

        index = (index + 1) & (gMemoryTrackingCapacity - 1);
    }
}

static bool memoryTrackingRemoveLocked(void* ptr, size_t* requestedSize, size_t* actualSize, size_t* alignment)
{
    MemoryTrackingEntry* entry = memoryTrackingFindEntryLocked(ptr);
    if (!entry)
        return false;

    if (requestedSize)
        *requestedSize = entry->mRequestedSize;
    if (actualSize)
        *actualSize = entry->mActualSize;
    if (alignment)
        *alignment = entry->mAlignment;

    memoryTrackingApplyFreeLocked(entry->mRequestedSize, entry->mActualSize);
    entry->pPtr = NULL;
    entry->mRequestedSize = 0;
    entry->mActualSize = 0;
    entry->mAlignment = 0;
    entry->mState = MEMORY_TRACKING_ENTRY_TOMBSTONE;
    --gMemoryTrackingCount;
    ++gMemoryTrackingTombstoneCount;
    return true;
}

static void memoryTrackingReset(void)
{
    memoryTrackingLock();
    free(gMemoryTrackingEntries);
    gMemoryTrackingEntries = NULL;
    gMemoryTrackingCapacity = 0;
    gMemoryTrackingCount = 0;
    gMemoryTrackingTombstoneCount = 0;
    memset(&gMemoryTrackingStats, 0, sizeof(gMemoryTrackingStats));
    gMemoryTrackingStats.mTrackingEnabled = true;
    memoryTrackingUnlock();
}

static size_t memoryTrackingGetUsableSize(void* ptr, size_t requestedSize, size_t alignment)
{
    if (!ptr)
        return requestedSize;

#if defined(_MSC_VER)
    size_t actualSize = _aligned_msize(ptr, alignment ? alignment : MIN_ALLOC_ALIGNMENT, 0);
    return actualSize >= requestedSize ? actualSize : requestedSize;
#elif defined(__linux__)
    size_t actualSize = malloc_usable_size(ptr);
    return actualSize >= requestedSize ? actualSize : requestedSize;
#else
    UNREF_PARAM(alignment);
    return requestedSize;
#endif
}

static void memoryTrackingRecordFailedAlloc(void)
{
    memoryTrackingLock();
    gMemoryTrackingStats.mTrackingEnabled = true;
    ++gMemoryTrackingStats.mFailedAllocationCount;
    memoryTrackingUnlock();
}

static void memoryTrackingRecordAlloc(void* ptr, size_t requestedSize, size_t actualSize, size_t alignment)
{
    if (!ptr)
    {
        memoryTrackingRecordFailedAlloc();
        return;
    }

    if (actualSize < requestedSize)
        actualSize = requestedSize;

    bool inserted = false;
    bool replaced = false;
    memoryTrackingLock();
    gMemoryTrackingStats.mTrackingEnabled = true;
    inserted = memoryTrackingInsertLocked(ptr, requestedSize, actualSize, alignment, &replaced);
    if (!inserted)
        ++gMemoryTrackingStats.mFailedAllocationCount;
    memoryTrackingUnlock();

    if (inserted)
    {
        if (replaced)
            TracyCFreeNS(ptr, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, TF_MEMORY_TRACY_POOL);
        TracyCAllocNS(ptr, requestedSize, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, TF_MEMORY_TRACY_POOL);
    }
}

static void memoryTrackingRecordFree(void* ptr)
{
    if (!ptr)
        return;

    bool found = false;
    memoryTrackingLock();
    found = memoryTrackingRemoveLocked(ptr, NULL, NULL, NULL);
    memoryTrackingUnlock();

    if (found)
        TracyCFreeNS(ptr, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, TF_MEMORY_TRACY_POOL);
}

static void memoryTrackingRecordRealloc(void* oldPtr, void* newPtr, size_t requestedSize, size_t actualSize, size_t alignment)
{
    // The caller holds the lock across realloc so another thread cannot reuse oldPtr
    // before its tracking entry and Tracy event have been retired.
    if (!newPtr)
    {
        ++gMemoryTrackingStats.mFailedAllocationCount;
        memoryTrackingUnlock();
        return;
    }

    size_t oldRequestedSize = 0;
    size_t oldActualSize = 0;
    size_t oldAlignment = 0;
    bool   removed = false;
    bool   inserted = false;
    bool   replaced = false;

    if (actualSize < requestedSize)
        actualSize = requestedSize;

    removed = memoryTrackingRemoveLocked(oldPtr, &oldRequestedSize, &oldActualSize, &oldAlignment);
    inserted = memoryTrackingInsertLocked(newPtr, requestedSize, actualSize, alignment, &replaced);
    if (inserted)
    {
        gMemoryTrackingStats.mTrackingEnabled = true;
        if (oldPtr)
            ++gMemoryTrackingStats.mReallocationCount;
    }
    else
    {
        ++gMemoryTrackingStats.mFailedAllocationCount;
    }
    UNREF_PARAM(oldRequestedSize);
    UNREF_PARAM(oldActualSize);
    UNREF_PARAM(oldAlignment);

    if (removed)
        TracyCFreeNS(oldPtr, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, TF_MEMORY_TRACY_POOL);
    if (replaced && newPtr != oldPtr)
        TracyCFreeNS(newPtr, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, TF_MEMORY_TRACY_POOL);
    if (inserted)
        TracyCAllocNS(newPtr, requestedSize, HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH, TF_MEMORY_TRACY_POOL);
    memoryTrackingUnlock();
}

static int64_t memoryTrackingPlotValue(uint64_t value) { return value > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)value; }

#endif // defined(ENABLE_TRACY_MEMORY)

MemoryTrackingStats memGetTrackingStats(void)
{
    MemoryTrackingStats stats;
    memset(&stats, 0, sizeof(stats));

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingLock();
    stats = gMemoryTrackingStats;
    stats.mTrackingEnabled = true;
    stats.mLiveSlackBytes = stats.mLiveActualBytes > stats.mLiveRequestedBytes ? stats.mLiveActualBytes - stats.mLiveRequestedBytes : 0;
    stats.mFragmentationPercent = stats.mLiveActualBytes ? ((float)stats.mLiveSlackBytes * 100.0f) / (float)stats.mLiveActualBytes : 0.0f;
    memoryTrackingUnlock();
#endif

    return stats;
}

void memPlotTrackingStats(void)
{
#if defined(ENABLE_TRACY_MEMORY)
    static bool plotsConfigured = false;
    if (!plotsConfigured)
    {
        TracyCPlotConfig("CPU/tf Requested", TracyPlotFormatMemory, 0, 1, 0x4E79A7);
        TracyCPlotConfig("CPU/tf Usable", TracyPlotFormatMemory, 0, 1, 0x59A14F);
        TracyCPlotConfig("CPU/tf Slack", TracyPlotFormatMemory, 0, 1, 0xE15759);
        TracyCPlotConfig("CPU/tf Fragmentation %", TracyPlotFormatPercentage, 0, 1, 0xF28E2B);
        TracyCPlotConfig("CPU/tf Live Allocations", TracyPlotFormatNumber, 0, 1, 0xB07AA1);
        plotsConfigured = true;
    }

    MemoryTrackingStats stats = memGetTrackingStats();
    TracyCPlotI("CPU/tf Requested", memoryTrackingPlotValue(stats.mLiveRequestedBytes));
    TracyCPlotI("CPU/tf Usable", memoryTrackingPlotValue(stats.mLiveActualBytes));
    TracyCPlotI("CPU/tf Slack", memoryTrackingPlotValue(stats.mLiveSlackBytes));
    TracyCPlot("CPU/tf Fragmentation %", stats.mFragmentationPercent);
    TracyCPlotI("CPU/tf Live Allocations", memoryTrackingPlotValue(stats.mLiveAllocationCount));
#endif
}

#if defined(ENABLE_MEMORY_TRACKING)

#define _CRT_SECURE_NO_WARNINGS 1

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment" // Do not warn whenever a comment-start sequence /* appears in a /* comment, or whenever a
                                           // backslash-newline appears in a // comment.
#pragma GCC diagnostic ignored "-Wformat-truncation" // Do not warn about calls to formatted input/output functions such as snprintf and
                                                     // vsnprintf that might result in output truncation.
#pragma GCC diagnostic ignored \
    "-Wstringop-truncation" // Do not warn for calls to bounded string manipulation functions such as strncat, strncpy, and stpncpy that may
                            // either truncate the copied string or leave the destination unchanged.
#endif

// Just include the cpp here so we don't have to add it to the all projects
#include <ThirdParty/MemoryManager/mmgr.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

void* tf_malloc_internal(size_t size, const char* f, int l, const char* sf)
{
    return tf_memalign_internal(MIN_ALLOC_ALIGNMENT, size, f, l, sf);
}

void* tf_calloc_internal(size_t count, size_t size, const char* f, int l, const char* sf)
{
    return tf_calloc_memalign_internal(count, MIN_ALLOC_ALIGNMENT, size, f, l, sf);
}

void* tf_memalign_internal(size_t align, size_t size, const char* f, int l, const char* sf)
{
    void* pMemAlign = mmgrAllocator(f, l, sf, m_alloc_malloc, align, size);

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(pMemAlign, size, size, align);
#endif

    // Return handle to allocated memory.
    return pMemAlign;
}

void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf)
{
    size_t requestedSize = 0;
    if (!tf_mul_size(count, size, &requestedSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    size_t alignedSize = ALIGN_TO(size, align);
    size_t allocationSize = 0;
    if (!tf_mul_size(count, alignedSize, &allocationSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    void* pMemAlign = mmgrAllocator(f, l, sf, m_alloc_calloc, align, allocationSize);

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(pMemAlign, requestedSize, allocationSize, align);
#endif

    // Return handle to allocated memory.
    return pMemAlign;
}

void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf)
{
#if defined(ENABLE_TRACY_MEMORY)
    if (ptr && !size)
    {
        tf_free_internal(ptr, f, l, sf);
        return NULL;
    }
    memoryTrackingLock();
#endif
    void* pRealloc = mmgrReallocator(f, l, sf, m_alloc_realloc, size, ptr);

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordRealloc(ptr, pRealloc, size, size, MIN_ALLOC_ALIGNMENT);
#endif

    // Return handle to reallocated memory.
    return pRealloc;
}

void tf_free_internal(void* ptr, const char* f, int l, const char* sf)
{
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordFree(ptr);
#endif
    mmgrDeallocator(f, l, sf, m_alloc_free, ptr);
}

#else // defined(ENABLE_MEMORY_TRACKING)

bool initMemAlloc(const char* appName)
{
    UNREF_PARAM(appName);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingReset();
#endif
    // No op but this is where you would initialize your memory allocator and bookkeeping data in a real world scenario
    return true;
}

void exitMemAlloc(void)
{
    // Return all allocated memory to the OS. Analyze memory usage, dump memory leaks, ...
}

void* tf_malloc_(size_t size)
{
#ifdef _MSC_VER
    void* ptr = _aligned_malloc(size, MIN_ALLOC_ALIGNMENT);
#else
    void* ptr = malloc(size);
#endif

    return ptr;
}

void* tf_calloc_(size_t count, size_t size)
{
    size_t sz = 0;
    if (!tf_mul_size(count, size, &sz))
        return NULL;

#ifdef _MSC_VER
    void*  ptr = tf_malloc_(sz);
    if (ptr)
        memset(ptr, 0, sz); //-V575
#else
    void* ptr = calloc(count, size);
#endif

    return ptr;
}

void* tf_memalign_(size_t alignment, size_t size)
{
#ifdef _MSC_VER
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr;
    alignment = alignment > sizeof(void*) ? alignment : sizeof(void*);
    if (posix_memalign(&ptr, alignment, size))
    {
        ptr = NULL;
    }
#endif

    return ptr;
}

void* tf_calloc_memalign_(size_t count, size_t alignment, size_t size)
{
    size_t alignedArrayElementSize = ALIGN_TO(size, alignment);
    size_t totalBytes = 0;
    if (!tf_mul_size(count, alignedArrayElementSize, &totalBytes))
        return NULL;

    void* ptr = tf_memalign_(alignment, totalBytes);

    if (ptr)
        memset(ptr, 0, totalBytes); //-V575
    return ptr;
}

void* tf_realloc_(void* ptr, size_t size)
{
#ifdef _MSC_VER
    void* reallocPtr = _aligned_realloc(ptr, size, MIN_ALLOC_ALIGNMENT);
#else
    void* reallocPtr = realloc(ptr, size);
#endif

    return reallocPtr;
}

void tf_free_(void* ptr)
{
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void* tf_malloc_internal(size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    void* ptr = tf_malloc_(size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, size, memoryTrackingGetUsableSize(ptr, size, MIN_ALLOC_ALIGNMENT), MIN_ALLOC_ALIGNMENT);
#endif
    return ptr;
}

void* tf_memalign_internal(size_t align, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    void* ptr = tf_memalign_(align, size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, size, memoryTrackingGetUsableSize(ptr, size, align), align);
#endif
    return ptr;
}

void* tf_calloc_internal(size_t count, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    size_t requestedSize = 0;
    if (!tf_mul_size(count, size, &requestedSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    void* ptr = tf_calloc_(count, size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, requestedSize, memoryTrackingGetUsableSize(ptr, requestedSize, MIN_ALLOC_ALIGNMENT),
                              MIN_ALLOC_ALIGNMENT);
#endif
    return ptr;
}

void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    size_t requestedSize = 0;
    if (!tf_mul_size(count, size, &requestedSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    void* ptr = tf_calloc_memalign_(count, align, size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, requestedSize, memoryTrackingGetUsableSize(ptr, requestedSize, align), align);
#endif
    return ptr;
}

void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
#if defined(ENABLE_TRACY_MEMORY)
    if (ptr && !size)
    {
        tf_free_internal(ptr, f, l, sf);
        return NULL;
    }
    memoryTrackingLock();
#endif
    void* reallocPtr = tf_realloc_(ptr, size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordRealloc(ptr, reallocPtr, size, memoryTrackingGetUsableSize(reallocPtr, size, MIN_ALLOC_ALIGNMENT),
                                MIN_ALLOC_ALIGNMENT);
#endif
    return reallocPtr;
}

void tf_free_internal(void* ptr, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordFree(ptr);
#endif
    tf_free_(ptr);
}

#endif // defined(ENABLE_MEMORY_TRACKING)
