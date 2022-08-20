#include <runtime/function/rhi/Buffer.h>

namespace Horizon::RHI {

Buffer::Buffer(const BufferCreateInfo &buffer_create_info) noexcept
    : m_size(buffer_create_info.size), m_descriptor_type(buffer_create_info.descriptor_type),
      m_resource_state(buffer_create_info.initial_state) {}

} // namespace Horizon::RHI
