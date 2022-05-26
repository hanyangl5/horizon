#include "Buffer.h"

namespace Horizon {
	namespace RHI {
		Buffer::Buffer(const BufferCreateInfo& buffer_create_info) :m_size(buffer_create_info.size), m_usage(buffer_create_info.buffer_usage_flags)
		{
		}
		u32 Horizon::RHI::Buffer::GetBufferSize()
		{
			return m_size;
		}

	}
}

