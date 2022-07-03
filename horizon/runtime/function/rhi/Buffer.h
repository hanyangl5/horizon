#pragma once

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {

class Buffer {
  public:
    Buffer(const BufferCreateInfo &buffer_create_info) noexcept;
    virtual ~Buffer() noexcept = default;
    u64 GetBufferSize() const noexcept;
    u32 GetBufferUsage() const noexcept;
    virtual void *GetBufferPointer() noexcept = 0;
    bool &Initialized() noexcept;

  protected:
    u32 m_usage{};
    u32 m_size{};
    bool m_initialized{false};
};
} // namespace Horizon::RHI
