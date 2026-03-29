#pragma once

#include <rhi/metal/metal_utils.h>

namespace Horizon::Backend
{

class MetalBuffer final : public Buffer
{
  public:
    MetalBuffer(const BufferCreateInfo &buffer_create_info, MTL::Device *device) noexcept;
    ~MetalBuffer() noexcept override;
    MetalBuffer(const MetalBuffer &rhs) noexcept = delete;
    MetalBuffer &operator=(const MetalBuffer &rhs) noexcept = delete;
    MetalBuffer(MetalBuffer &&rhs) noexcept = delete;
    MetalBuffer &operator=(MetalBuffer &&rhs) noexcept = delete;

    MTL::Buffer *m_buffer{};
};

} // namespace Horizon::Backend
