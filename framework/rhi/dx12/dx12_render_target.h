#pragma once

#include <d3d12.h>
#include "dx12_utils.h"

#include <core/definations.h>
#include <rhi/render_target.h>

namespace Horizon::Backend
{

class DX12RenderTarget : public RenderTarget
{
  public:
    DX12RenderTarget(const DX12RendererContext &context,
                     const RenderTargetCreateInfo &render_target_create_info) noexcept;
    virtual ~DX12RenderTarget() noexcept;
    DX12RenderTarget(const DX12RenderTarget &rhs) noexcept = delete;
    DX12RenderTarget &operator=(const DX12RenderTarget &rhs) noexcept = delete;
    DX12RenderTarget(DX12RenderTarget &&rhs) noexcept = delete;
    DX12RenderTarget &operator=(DX12RenderTarget &&rhs) noexcept = delete;

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const noexcept
    {
        return m_rtv_handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const noexcept
    {
        return m_dsv_handle;
    }

  public:
    const DX12RendererContext &m_context;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtv_handle{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsv_handle{};
};

} // namespace Horizon::Backend
