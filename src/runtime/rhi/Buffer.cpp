#include "Buffer.h"

namespace Horizon::Backend {

Buffer::Buffer(const BufferCreateInfo &buffer_create_info) noexcept
    : m_descriptor_types(buffer_create_info.descriptor_types), m_resource_state(buffer_create_info.initial_state),
      m_size(buffer_create_info.size) {}

} // namespace Horizon::Backend
