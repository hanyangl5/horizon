
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

#define ALIGN_TO(Size, alignment) (((Size) + (alignment)-1) & ~((alignment)-1))

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

static bool MulSize(size_t lhs, size_t rhs, size_t* result)
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
    size_t  requestedSize;
    size_t  actualSize;
    size_t  alignment;
    uint8_t state;
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
    uintptr_t Hash = (uintptr_t)ptr;
    Hash >>= 4;
#if UINTPTR_MAX > 0xFFFFFFFFu
    Hash ^= Hash >> 33;
    Hash *= (uintptr_t)0xff51afd7ed558ccdull;
    Hash ^= Hash >> 33;
    Hash *= (uintptr_t)0xc4ceb9fe1a85ec53ull;
    Hash ^= Hash >> 33;
#else
    Hash ^= Hash >> 16;
    Hash *= (uintptr_t)0x7feb352du;
    Hash ^= Hash >> 15;
    Hash *= (uintptr_t)0x846ca68bu;
    Hash ^= Hash >> 16;
#endif
    return (size_t)Hash;
}

static void memoryTrackingApplyAllocLocked(size_t requestedSize, size_t actualSize)
{
    uint64_t requested = (uint64_t)requestedSize;
    uint64_t actual = (uint64_t)actualSize;

    if (actual < requested)
        actual = requested;

    gMemoryTrackingStats.LiveRequestedBytes += requested;
    gMemoryTrackingStats.LiveActualBytes += actual;
    ++gMemoryTrackingStats.LiveAllocationCount;
    ++gMemoryTrackingStats.TotalAllocationCount;

    if (gMemoryTrackingStats.LiveRequestedBytes > gMemoryTrackingStats.PeakRequestedBytes)
        gMemoryTrackingStats.PeakRequestedBytes = gMemoryTrackingStats.LiveRequestedBytes;
    if (gMemoryTrackingStats.LiveActualBytes > gMemoryTrackingStats.PeakActualBytes)
        gMemoryTrackingStats.PeakActualBytes = gMemoryTrackingStats.LiveActualBytes;
    if (gMemoryTrackingStats.LiveAllocationCount > gMemoryTrackingStats.PeakAllocationCount)
        gMemoryTrackingStats.PeakAllocationCount = gMemoryTrackingStats.LiveAllocationCount;
}

static void memoryTrackingApplyFreeLocked(size_t requestedSize, size_t actualSize)
{
    uint64_t requested = (uint64_t)requestedSize;
    uint64_t actual = (uint64_t)actualSize;

    if (actual < requested)
        actual = requested;

    if (gMemoryTrackingStats.LiveRequestedBytes >= requested)
        gMemoryTrackingStats.LiveRequestedBytes -= requested;
    else
        gMemoryTrackingStats.LiveRequestedBytes = 0;

    if (gMemoryTrackingStats.LiveActualBytes >= actual)
        gMemoryTrackingStats.LiveActualBytes -= actual;
    else
        gMemoryTrackingStats.LiveActualBytes = 0;

    if (gMemoryTrackingStats.LiveAllocationCount > 0)
        --gMemoryTrackingStats.LiveAllocationCount;
}

static MemoryTrackingEntry* memoryTrackingFindEntryLocked(void* ptr)
{
    if (!ptr || !gMemoryTrackingEntries || gMemoryTrackingCapacity == 0)
        return NULL;

    size_t index = memoryTrackingHashPtr(ptr) & (gMemoryTrackingCapacity - 1);
    for (size_t probe = 0; probe < gMemoryTrackingCapacity; ++probe)
    {
        MemoryTrackingEntry* entry = &gMemoryTrackingEntries[index];
        if (entry->state == MEMORY_TRACKING_ENTRY_EMPTY)
            return NULL;
        if (entry->state == MEMORY_TRACKING_ENTRY_OCCUPIED && entry->pPtr == ptr)
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
        if (oldEntry->state != MEMORY_TRACKING_ENTRY_OCCUPIED)
            continue;

        size_t index = memoryTrackingHashPtr(oldEntry->pPtr) & (gMemoryTrackingCapacity - 1);
        while (gMemoryTrackingEntries[index].state == MEMORY_TRACKING_ENTRY_OCCUPIED)
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
        memoryTrackingApplyFreeLocked(existing->requestedSize, existing->actualSize);
        existing->requestedSize = requestedSize;
        existing->actualSize = actualSize;
        existing->alignment = alignment;
        memoryTrackingApplyAllocLocked(requestedSize, actualSize);
        return true;
    }

    size_t               index = memoryTrackingHashPtr(ptr) & (gMemoryTrackingCapacity - 1);
    MemoryTrackingEntry* tombstone = NULL;

    for (;;)
    {
        MemoryTrackingEntry* entry = &gMemoryTrackingEntries[index];
        if (entry->state == MEMORY_TRACKING_ENTRY_EMPTY)
        {
            if (tombstone)
                entry = tombstone;

            entry->pPtr = ptr;
            entry->requestedSize = requestedSize;
            entry->actualSize = actualSize;
            entry->alignment = alignment;
            entry->state = MEMORY_TRACKING_ENTRY_OCCUPIED;
            ++gMemoryTrackingCount;
            if (entry == tombstone && gMemoryTrackingTombstoneCount > 0)
                --gMemoryTrackingTombstoneCount;
            memoryTrackingApplyAllocLocked(requestedSize, actualSize);
            return true;
        }
        if (entry->state == MEMORY_TRACKING_ENTRY_TOMBSTONE && !tombstone)
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
        *requestedSize = entry->requestedSize;
    if (actualSize)
        *actualSize = entry->actualSize;
    if (alignment)
        *alignment = entry->alignment;

    memoryTrackingApplyFreeLocked(entry->requestedSize, entry->actualSize);
    entry->pPtr = NULL;
    entry->requestedSize = 0;
    entry->actualSize = 0;
    entry->alignment = 0;
    entry->state = MEMORY_TRACKING_ENTRY_TOMBSTONE;
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
    gMemoryTrackingStats.TrackingEnabled = true;
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
    gMemoryTrackingStats.TrackingEnabled = true;
    ++gMemoryTrackingStats.FailedAllocationCount;
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
    gMemoryTrackingStats.TrackingEnabled = true;
    inserted = memoryTrackingInsertLocked(ptr, requestedSize, actualSize, alignment, &replaced);
    if (!inserted)
        ++gMemoryTrackingStats.FailedAllocationCount;
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
        ++gMemoryTrackingStats.FailedAllocationCount;
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
        gMemoryTrackingStats.TrackingEnabled = true;
        if (oldPtr)
            ++gMemoryTrackingStats.ReallocationCount;
    }
    else
    {
        ++gMemoryTrackingStats.FailedAllocationCount;
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

MemoryTrackingStats GetMemoryTrackingStats(void)
{
    MemoryTrackingStats stats;
    memset(&stats, 0, sizeof(stats));

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingLock();
    stats = gMemoryTrackingStats;
    stats.TrackingEnabled = true;
    stats.LiveSlackBytes = stats.LiveActualBytes > stats.LiveRequestedBytes ? stats.LiveActualBytes - stats.LiveRequestedBytes : 0;
    stats.FragmentationPercent = stats.LiveActualBytes ? ((float)stats.LiveSlackBytes * 100.0f) / (float)stats.LiveActualBytes : 0.0f;
    memoryTrackingUnlock();
#endif

    return stats;
}

void PlotMemoryTrackingStats(void)
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

    MemoryTrackingStats stats = GetMemoryTrackingStats();
    TracyCPlotI("CPU/tf Requested", memoryTrackingPlotValue(stats.LiveRequestedBytes));
    TracyCPlotI("CPU/tf Usable", memoryTrackingPlotValue(stats.LiveActualBytes));
    TracyCPlotI("CPU/tf Slack", memoryTrackingPlotValue(stats.LiveSlackBytes));
    TracyCPlot("CPU/tf Fragmentation %", stats.FragmentationPercent);
    TracyCPlotI("CPU/tf Live Allocations", memoryTrackingPlotValue(stats.LiveAllocationCount));
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

void* MemAllocateInternal(size_t Size, const char* f, int l, const char* sf)
{
    return MemAllocateAlignedInternal(MIN_ALLOC_ALIGNMENT, Size, f, l, sf);
}

void* MemCallocateInternal(size_t Count, size_t Size, const char* f, int l, const char* sf)
{
    return MemCallocateAlignedInternal(Count, MIN_ALLOC_ALIGNMENT, Size, f, l, sf);
}

void* MemAllocateAlignedInternal(size_t align, size_t Size, const char* f, int l, const char* sf)
{
    void* pMemAlign = mmgrAllocator(f, l, sf, m_alloc_malloc, align, Size);

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(pMemAlign, Size, Size, align);
#endif

    // Return Handle to allocated memory.
    return pMemAlign;
}

void* MemCallocateAlignedInternal(size_t Count, size_t align, size_t Size, const char* f, int l, const char* sf)
{
    size_t requestedSize = 0;
    if (!MulSize(Count, Size, &requestedSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    size_t alignedSize = ALIGN_TO(Size, align);
    size_t allocationSize = 0;
    if (!MulSize(Count, alignedSize, &allocationSize))
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

    // Return Handle to allocated memory.
    return pMemAlign;
}

void* MemReallocateInternal(void* ptr, size_t Size, const char* f, int l, const char* sf)
{
#if defined(ENABLE_TRACY_MEMORY)
    if (ptr && !Size)
    {
        MemDeallocateInternal(ptr, f, l, sf);
        return NULL;
    }
    memoryTrackingLock();
#endif
    void* pRealloc = mmgrReallocator(f, l, sf, m_alloc_realloc, Size, ptr);

#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordRealloc(ptr, pRealloc, Size, Size, MIN_ALLOC_ALIGNMENT);
#endif

    // Return Handle to reallocated memory.
    return pRealloc;
}

void MemDeallocateInternal(void* ptr, const char* f, int l, const char* sf)
{
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordFree(ptr);
#endif
    mmgrDeallocator(f, l, sf, m_alloc_free, ptr);
}

#else // defined(ENABLE_MEMORY_TRACKING)

bool InitMemoryAlloc(const char* appName)
{
    UNREF_PARAM(appName);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingReset();
#endif
    // No op but this is where you would initialize your memory allocator and bookkeeping data in a real world scenario
    return true;
}

void ExitMemoryAlloc(void)
{
    // Return all allocated memory to the OS. Analyze memory usage, dump memory leaks, ...
}

void* TrackedMalloc(size_t Size)
{
#ifdef _MSC_VER
    void* ptr = _aligned_malloc(Size, MIN_ALLOC_ALIGNMENT);
#else
    void* ptr = malloc(Size);
#endif

    return ptr;
}

void* TrackedCalloc(size_t Count, size_t Size)
{
    size_t sz = 0;
    if (!MulSize(Count, Size, &sz))
        return NULL;

#ifdef _MSC_VER
    void* ptr = TrackedMalloc(sz);
    if (ptr)
        memset(ptr, 0, sz); //-V575
#else
    void* ptr = calloc(Count, Size);
#endif

    return ptr;
}

void* TrackedMemalign(size_t alignment, size_t Size)
{
#ifdef _MSC_VER
    void* ptr = _aligned_malloc(Size, alignment);
#else
    void* ptr;
    alignment = alignment > sizeof(void*) ? alignment : sizeof(void*);
    if (posix_memalign(&ptr, alignment, Size))
    {
        ptr = NULL;
    }
#endif

    return ptr;
}

void* TrackedCallocMemalign(size_t Count, size_t alignment, size_t Size)
{
    size_t alignedArrayElementSize = ALIGN_TO(Size, alignment);
    size_t totalBytes = 0;
    if (!MulSize(Count, alignedArrayElementSize, &totalBytes))
        return NULL;

    void* ptr = TrackedMemalign(alignment, totalBytes);

    if (ptr)
        memset(ptr, 0, totalBytes); //-V575
    return ptr;
}

void* TrackedRealloc(void* ptr, size_t Size)
{
#ifdef _MSC_VER
    void* reallocPtr = _aligned_realloc(ptr, Size, MIN_ALLOC_ALIGNMENT);
#else
    void* reallocPtr = realloc(ptr, Size);
#endif

    return reallocPtr;
}

void TrackedFree(void* ptr)
{
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void* MemAllocateInternal(size_t Size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    void* ptr = TrackedMalloc(Size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, Size, memoryTrackingGetUsableSize(ptr, Size, MIN_ALLOC_ALIGNMENT), MIN_ALLOC_ALIGNMENT);
#endif
    return ptr;
}

void* MemAllocateAlignedInternal(size_t align, size_t Size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    void* ptr = TrackedMemalign(align, Size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, Size, memoryTrackingGetUsableSize(ptr, Size, align), align);
#endif
    return ptr;
}

void* MemCallocateInternal(size_t Count, size_t Size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    size_t requestedSize = 0;
    if (!MulSize(Count, Size, &requestedSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    void* ptr = TrackedCalloc(Count, Size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, requestedSize, memoryTrackingGetUsableSize(ptr, requestedSize, MIN_ALLOC_ALIGNMENT),
                              MIN_ALLOC_ALIGNMENT);
#endif
    return ptr;
}

void* MemCallocateAlignedInternal(size_t Count, size_t align, size_t Size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    size_t requestedSize = 0;
    if (!MulSize(Count, Size, &requestedSize))
    {
#if defined(ENABLE_TRACY_MEMORY)
        memoryTrackingRecordFailedAlloc();
#endif
        return NULL;
    }

    void* ptr = TrackedCallocMemalign(Count, align, Size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordAlloc(ptr, requestedSize, memoryTrackingGetUsableSize(ptr, requestedSize, align), align);
#endif
    return ptr;
}

void* MemReallocateInternal(void* ptr, size_t Size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
#if defined(ENABLE_TRACY_MEMORY)
    if (ptr && !Size)
    {
        MemDeallocateInternal(ptr, f, l, sf);
        return NULL;
    }
    memoryTrackingLock();
#endif
    void* reallocPtr = TrackedRealloc(ptr, Size);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordRealloc(ptr, reallocPtr, Size, memoryTrackingGetUsableSize(reallocPtr, Size, MIN_ALLOC_ALIGNMENT),
                                MIN_ALLOC_ALIGNMENT);
#endif
    return reallocPtr;
}

void MemDeallocateInternal(void* ptr, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
#if defined(ENABLE_TRACY_MEMORY)
    memoryTrackingRecordFree(ptr);
#endif
    TrackedFree(ptr);
}

#endif // defined(ENABLE_MEMORY_TRACKING)
