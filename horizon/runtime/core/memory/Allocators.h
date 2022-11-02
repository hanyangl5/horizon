﻿#pragma once

#include <memory>
#include <memory_resource>

#ifndef NDEBUG
 #define MEMORY_RESOURCE_TRACKING
#endif

namespace Horizon::Memory {

class GlobalMemoryAllocator : public std::pmr::memory_resource {
  public:
    GlobalMemoryAllocator() noexcept;
    ~GlobalMemoryAllocator() noexcept;

  private:
    void *do_allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) override;

    void do_deallocate(void *ptr, size_t bytes, size_t alignment = alignof(std::max_align_t)) override;

    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override;
};

//
//class TemporalMemoryAllocator : public std::pmr::memory_resource {
//  public:
//    TemporalMemoryAllocator() noexcept;
//    ~TemporalMemoryAllocator() noexcept;
//
//  private:
//    void *do_allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) override;
//
//    void do_deallocate(void *ptr, size_t bytes, size_t alignment = alignof(std::max_align_t)) override;
//
//    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override;
//};

} // namespace Horizon::Memory