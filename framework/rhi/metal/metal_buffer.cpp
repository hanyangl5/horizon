#include "metal_buffer.h"

namespace Horizon::Backend
{

using namespace MetalUtils;

MetalBuffer::MetalBuffer(const BufferCreateInfo &buffer_create_info, MTL::Device *device) noexcept
    : Buffer(buffer_create_info), m_buffer(device->newBuffer(buffer_create_info.size, MTL::ResourceStorageModeShared))
{
    if (m_buffer != nil && !m_debug_name.empty())
    {
        m_buffer->setLabel(ToNSString(m_debug_name));
    }
}

MetalBuffer::~MetalBuffer() noexcept
{
    if (m_buffer != nil)
    {
        m_buffer->release();
        m_buffer = nil;
    }
}

} // namespace Horizon::Backend
