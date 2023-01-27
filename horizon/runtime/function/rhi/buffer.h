#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/rhi_utils.h>

namespace Horizon::Backend {

class Buffer {
  public:
    Buffer(const BufferCreateInfo &buffer_create_info) noexcept;
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
