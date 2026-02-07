#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "dx12_utils.h"

#include <core/definations.h>
#include <rhi/enums.h>

using Microsoft::WRL::ComPtr;

namespace Horizon::Backend
{

class DX12CommandAllocatorPool
{
  public:
    DX12CommandAllocatorPool(const DX12RendererContext &context, CommandQueueType queue_type) noexcept;
    ~DX12CommandAllocatorPool() noexcept;

    ComPtr<ID3D12CommandAllocator> GetAllocator();
    void ResetAllocator(ComPtr<ID3D12CommandAllocator> allocator);
    void ResetAll();

  private:
    const DX12RendererContext &m_context;
    CommandQueueType m_queue_type;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_allocators;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_available_allocators;
    u32 m_current_index{0};
};

} // namespace Horizon::Backend
