#pragma once

#include <runtime/core/utils/Definations.h>

#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::Backend {

class Buffer {
  public:
    using allocator_type = std::pmr::polymorphic_allocator<std::byte>;
    allocator_type get_allocator() const noexcept;
  public:
    Buffer(const BufferCreateInfo &buffer_create_info, const allocator_type &alloc = {}) noexcept;
    virtual ~Buffer() noexcept = default;
    Buffer(const Buffer &rhs) noexcept = delete;
    Buffer &operator=(const Buffer &rhs) noexcept = delete;
    Buffer(Buffer &&rhs) noexcept = delete;
    Buffer &operator=(Buffer &&rhs) noexcept = delete;
  public:
    const DescriptorTypes m_descriptor_types{};
    ResourceState m_resource_state{};
    const u64 m_size{};
};
} // namespace Horizon::Backend
