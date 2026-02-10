#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/buffer.h>

namespace Horizon::Backend
{

class DX12Buffer : public Buffer
{
  public:
    DX12Buffer(const DX12RendererContext &context, const BufferCreateInfo &buffer_create_info) noexcept;
    virtual ~DX12Buffer() noexcept;
    DX12Buffer(const DX12Buffer &rhs) noexcept = delete;
    DX12Buffer &operator=(const DX12Buffer &rhs) noexcept = delete;
    DX12Buffer(DX12Buffer &&rhs) noexcept = delete;
    DX12Buffer &operator=(DX12Buffer &&rhs) noexcept = delete;

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const noexcept
    {
        return m_resource->GetGPUVirtualAddress();
    }

    ID3D12Resource *GetResource() const noexcept
    {
        return m_resource.Get();
    }

    // Get or create upload buffer for data uploads
    ID3D12Resource *GetUploadBuffer() noexcept;

  public:
    const DX12RendererContext &m_context;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_upload_buffer; // Upload buffer for UpdateBuffer operations
    D3D12_RESOURCE_STATES m_current_state;
};

} // namespace Horizon::Backend
