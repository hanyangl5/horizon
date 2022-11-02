#pragma once

#include <memory>
#include <memory_resource>

#include "Allocators.h"

namespace Horizon::Memory {

extern std::pmr::memory_resource *global_memory_resource;
// extern std::pmr::memory_resource *temporal_memory_resource;

void initialize();

void destroy();

template <typename T, typename... Args> T *Alloc(std::pmr::memory_resource &allocator, Args &&...args) {
    auto memory = allocator.allocate(sizeof(T), alignof(std::max_align_t));
    return new (memory) T(std::forward<Args>(args)...);
}

template <typename T, typename... Args> T *Alloc(Args &&...args) {
    return Alloc<T, Args...>(*Horizon::Memory::global_memory_resource, std::forward<Args>(args)...);
}

template <typename T> void Free(std::pmr::memory_resource &allocator, T *ptr) {
    if (!ptr)
        return;
    ptr->~T();
    allocator.deallocate(ptr, sizeof(T), alignof(std::max_align_t));
}

template <typename T> void Free(T *ptr) {
    if (!ptr)
        return;
    Free(*Horizon::Memory::global_memory_resource, ptr);
}

#define STACK_MEMORY_RESOURCE(size, memory) std::pmr::monotonic_buffer_resource memory(size);

inline std::pmr::monotonic_buffer_resource GetStackMemoryResource(size_t size = 1024) {
    return std::pmr::monotonic_buffer_resource(size);
}

} // namespace Horizon::Memory