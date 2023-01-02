/*****************************************************************//**
 * \file   memory.h
 * \brief  defination of the memory resources
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <memory>
#include <memory_resource>
#include <type_traits>
#include <xtr1common>

#include "Allocators.h"

namespace Horizon::Memory {

extern std::pmr::memory_resource *global_memory_resource;
// extern std::pmr::memory_resource *local_memory_resource;

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
    if (!ptr) {
        return;
    }
    ptr->~T();
    allocator.deallocate(ptr, sizeof(T), alignof(std::max_align_t));
}

template <typename T> void Free(T *ptr) {
    if (!ptr) {
        return;
    }
    Free(*Horizon::Memory::global_memory_resource, ptr);
}

inline std::pmr::memory_resource *GetGlobalAllocator() {
    return global_memory_resource; 
};

inline std::pmr::monotonic_buffer_resource GetStackMemoryResource(size_t size = 1024) {
    return std::pmr::monotonic_buffer_resource(size);
}

// smart_ptr with pmr

template <typename T> using UniquePtr = std::unique_ptr<T>;

template <typename T, typename... Args, std::enable_if_t<!std::is_array_v<T>, int> = 0>
_NODISCARD UniquePtr<T> MakeUnique(std::pmr::memory_resource &allocator, Args &&...args) { // make a unique_ptr
    void *memory = allocator.allocate(sizeof(T), alignof(std::max_align_t));
    T *object = new (memory) T(std::forward<Args>(args)...);
    return UniquePtr<T>(object);
}

template <typename T, typename... Args, std::enable_if_t<!std::is_array_v<T>, int> = 0>
_NODISCARD UniquePtr<T> MakeUnique(Args &&...args) { // make a unique_ptr
    return Memory::MakeUnique<T, Args...>(*Horizon::Memory::global_memory_resource, std::forward<Args>(args)...);
}

// we prefer using Container::Array than using unique_ptr<T[]>
// TODO(hylu): https://stackoverflow.com/questions/16711697/is-there-any-use-for-unique-ptr-with-array

// template <class T, std::enable_if_t<std::is_array_v<T> && std::extent_v<T> == 0, int> = 0>
//_NODISCARD UniquePtr<T> MakeUnique(std::pmr::memory_resource &allocator, const size_t _Size) { // make a unique_ptr
//     using _Elem = std::remove_extent_t<T>;
//     void *memory = allocator.allocate(sizeof(T) * _Size, alignof(std::max_align_t));
//     return UniquePtr<T>(new (memory) _Elem[_Size]());
// }

template <class T, class... Args, std::enable_if_t<std::extent_v<T> != 0, int> = 0>
void MakeUnique(Args &&...) = delete;

} // namespace Horizon::Memory