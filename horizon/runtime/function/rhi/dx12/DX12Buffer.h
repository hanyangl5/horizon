#pragma once

#include <D3D12MemAlloc.h>

#include "stdafx.h"
#include <runtime/function/rhi/Buffer.h>

namespace Horizon::RHI {
class DX12Buffer : public Buffer {
  public:
    DX12Buffer(D3D12MA::Allocator *allocator,
               const BufferCreateInfo &buffer_create_info,
               MemoryFlag memory_flag) noexcept;
    ~DX12Buffer() noexcept;
    virtual void *GetBufferPointer() noexcept override;

  private:
  public:
    D3D12MA::Allocation *m_allocation{};
    D3D12MA::Allocator *m_allocator{};
};
} // namespace Horizon::RHI
