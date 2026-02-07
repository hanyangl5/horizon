#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/texture.h>

using Microsoft::WRL::ComPtr;

namespace Horizon::Backend
{

class DX12Texture : public Texture
{
  public:
    DX12Texture(const DX12RendererContext &context, const TextureCreateInfo &texture_create_info) noexcept;
    virtual ~DX12Texture() noexcept;
    DX12Texture(const DX12Texture &rhs) noexcept = delete;
    DX12Texture &operator=(const DX12Texture &rhs) noexcept = delete;
    DX12Texture(DX12Texture &&rhs) noexcept = delete;
    DX12Texture &operator=(DX12Texture &&rhs) noexcept = delete;

    ID3D12Resource *GetResource() const noexcept
    {
        return m_resource.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const noexcept
    {
        return m_srv_handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetUAVHandle() const noexcept
    {
        return m_uav_handle;
    }

  public:
    const DX12RendererContext &m_context;
    ComPtr<ID3D12Resource> m_resource;
    D3D12_RESOURCE_STATES m_current_state;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srv_handle{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_uav_handle{};
};

} // namespace Horizon::Backend
