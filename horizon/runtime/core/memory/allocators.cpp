/*****************************************************************//**
 * \file   allocators.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "allocators.h"

// standard libraries

// third party libraries
#include <mimalloc.h>

// project headers
#include <runtime/core/log/log.h>

namespace Horizon::Memory {

GlobalMemoryAllocator::GlobalMemoryAllocator() noexcept {
}

GlobalMemoryAllocator::~GlobalMemoryAllocator() noexcept {
}

void *GlobalMemoryAllocator::do_allocate(size_t bytes, size_t alignment) {
#ifdef MEMORY_RESOURCE_TRACKING
    LOG_DEBUG("allocate {} Bytes with alignment {}", bytes, alignment);
#endif
    return mi_malloc_aligned(bytes, alignment);
}

void GlobalMemoryAllocator::do_deallocate(void *ptr, size_t bytes, size_t alignment) {
#ifdef MEMORY_RESOURCE_TRACKING
    LOG_DEBUG("deallocate {} Bytes with alignment {}", bytes, alignment);
#endif
    mi_free_aligned(ptr, alignment);
}

bool GlobalMemoryAllocator::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
    return this == &other;
}

//LocalMemoryAllocator::LocalMemoryAllocator() noexcept {
//}
//
//LocalMemoryAllocator::~LocalMemoryAllocator() noexcept {
//}
//
//void *LocalMemoryAllocator::do_allocate(size_t bytes, size_t alignment) { 
//    return nullptr; 
//}
//
//void LocalMemoryAllocator::do_deallocate(void *ptr, size_t bytes, size_t alignment) {
//}
//
//bool LocalMemoryAllocator::do_is_equal(const std::pmr::memory_resource &other) const noexcept { 
//    return false; 
//}

} // namespace Horizon::Memory