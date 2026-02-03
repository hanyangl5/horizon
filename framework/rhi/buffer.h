#pragma once

#include <core/definations.h>

#include <rhi/enums.h>

namespace Horizon::Backend
{

class Buffer
{
  public:
    Buffer(const BufferCreateInfo &buffer_create_info) noexcept;
    virtual ~Buffer() noexcept = default;

    // Set debug name for this buffer (for debugging tools like RenderDoc, Nsight, etc.)
    // virtual void SetDebugName(const char *name) = 0;

  public:
    std::string m_debug_name;
    const DescriptorTypes m_descriptor_types{};
    ResourceState m_resource_state{};
    const u64 m_size{};
};
} // namespace Horizon::Backend
