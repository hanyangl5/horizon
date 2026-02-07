#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "dx12_utils.h"

#include <core/definations.h>
#include <rhi/swap_chain.h>

using Microsoft::WRL::ComPtr;

namespace Horizon::Backend
{

class DX12SwapChain : public SwapChain
{
  public:
    DX12SwapChain(const DX12RendererContext &context, const SwapChainCreateInfo &swap_chain_create_info,
                  Window *window) noexcept;
    virtual ~DX12SwapChain() noexcept;
    DX12SwapChain(const DX12SwapChain &rhs) noexcept = delete;
    DX12SwapChain &operator=(const DX12SwapChain &rhs) noexcept = delete;
    DX12SwapChain(DX12SwapChain &&rhs) noexcept = delete;
    DX12SwapChain &operator=(DX12SwapChain &&rhs) noexcept = delete;

  public:
    const DX12RendererContext &m_context;
    ComPtr<IDXGISwapChain3> m_swap_chain;
    DXGI_FORMAT m_format;
    std::vector<ComPtr<ID3D12Resource>> m_back_buffers;
};

} // namespace Horizon::Backend
