
#include <Core/IConfig.h>

#include "wchar.h"

#include <memory.h>
#include <stdlib.h>

//#include "../ThirdParty/OpenSource/ModifiedSonyMath/vectormath_settings.hpp"
//#include <Core/IMath.h>
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

#include <Core/IMemory.h>

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

    // Return handle to allocated memory.
    return pMemAlign;
}

void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf)
{
    size = ALIGN_TO(size, align);

    void* pMemAlign = mmgrAllocator(f, l, sf, m_alloc_calloc, align, size * count);

    // Return handle to allocated memory.
    return pMemAlign;
}

void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf)
{
    void* pRealloc = mmgrReallocator(f, l, sf, m_alloc_realloc, size, ptr);

    // Return handle to reallocated memory.
    return pRealloc;
}

void tf_free_internal(void* ptr, const char* f, int l, const char* sf) { mmgrDeallocator(f, l, sf, m_alloc_free, ptr); }

#else // defined(ENABLE_MEMORY_TRACKING)

#include "stdbool.h"

bool initMemAlloc(const char* appName)
{
    UNREF_PARAM(appName);
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
#ifdef _MSC_VER
    size_t sz = count * size;
    void*  ptr = tf_malloc_(sz);
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
    size_t totalBytes = count * alignedArrayElementSize;

    void* ptr = tf_memalign_(alignment, totalBytes);

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
    return tf_malloc_(size);
}

void* tf_memalign_internal(size_t align, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_memalign_(align, size);
}

void* tf_calloc_internal(size_t count, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_calloc_(count, size);
}

void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_calloc_memalign_(count, align, size);
}

void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_realloc_(ptr, size);
}

void tf_free_internal(void* ptr, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    tf_free_(ptr);
}

#endif // defined(ENABLE_MEMORY_TRACKING)
