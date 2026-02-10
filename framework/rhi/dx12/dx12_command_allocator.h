#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/enums.h>

namespace Horizon::Backend
{

class DX12CommandAllocatorPool
{
  public:
    DX12CommandAllocatorPool(const DX12RendererContext &context, CommandQueueType queue_type) noexcept;
    ~DX12CommandAllocatorPool() noexcept;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetAllocator();
    void ResetAllocator(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);
    void ResetAll();

  private:
    const DX12RendererContext &m_context;
    CommandQueueType m_queue_type;
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_allocators;
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_available_allocators;
    [[maybe_unused]] u32 m_current_index{0};
};

} // namespace Horizon::Backend
