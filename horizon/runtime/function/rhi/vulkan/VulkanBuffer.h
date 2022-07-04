#pragma once

#include "vk_mem_alloc.h"

#include <runtime/function/rhi/Buffer.h>

namespace Horizon::RHI {

class VulkanBuffer : public Buffer {
  public:
    VulkanBuffer(VmaAllocator allocator,
                 const BufferCreateInfo &buffer_create_info,
                 MemoryFlag memory_flag) noexcept;
    virtual ~VulkanBuffer() noexcept;
    void *GetBufferPointer() noexcept override;

  private:

  public:
    VkBuffer m_buffer{};
    VmaAllocation m_memory{};
    VmaAllocator m_allocator{};
    VmaAllocationInfo m_allocation_info{};
};

} // namespace Horizon::RHI