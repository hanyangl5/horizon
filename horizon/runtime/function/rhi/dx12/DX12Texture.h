#pragma once

#include <D3D12MemAlloc.h>

#include "stdafx.h"
#include <runtime/function/rhi/Texture.h>

namespace Horizon::RHI {
class DX12Texture : public Texture {
  public:
    DX12Texture(D3D12MA::Allocator *allocator,
                const TextureCreateInfo &texture_create_info) noexcept;
    ~DX12Texture() noexcept;

  private:
    virtual void Destroy() noexcept override;

  private:
    D3D12MA::Allocation *m_allocation{};
    D3D12MA::Allocator *m_allocator{};
};
} // namespace Horizon::RHI