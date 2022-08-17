#pragma once

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {

class Buffer {
  public:
    Buffer(const BufferCreateInfo &buffer_create_info) noexcept;
    virtual ~Buffer() noexcept = default;

  public:
    const DescriptorType m_descriptor_type{};
    ResourceState m_resource_state{};
    const u64 m_size{};
};
} // namespace Horizon::RHI
