#include "Buffer.h"

namespace Horizon::Backend {

Buffer::Buffer(const BufferCreateInfo &buffer_create_info, const allocator_type &alloc) noexcept
    : m_size(buffer_create_info.size), m_descriptor_types(buffer_create_info.descriptor_types),
      m_resource_state(buffer_create_info.initial_state) {}

} // namespace Horizon::Backend
