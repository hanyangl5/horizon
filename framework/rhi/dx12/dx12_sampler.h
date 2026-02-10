#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include "dx12_descriptor_heap_allocator.h"
#include <core/definations.h>
#include <rhi/enums.h>
#include <rhi/sampler.h>

namespace Horizon::Backend
{

class DX12Sampler : public Sampler
{
  public:
    DX12Sampler(const DX12RendererContext &context, DX12DescriptorHeapAllocator &heap_allocator,
                const SamplerDesc &desc) noexcept;
    virtual ~DX12Sampler() noexcept;

    DX12Sampler(const DX12Sampler &rhs) noexcept = delete;
    DX12Sampler &operator=(const DX12Sampler &rhs) noexcept = delete;
    DX12Sampler(DX12Sampler &&rhs) noexcept = delete;
    DX12Sampler &operator=(DX12Sampler &&rhs) noexcept = delete;

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const noexcept
    {
        return m_cpu_handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const noexcept
    {
        return m_gpu_handle;
    }

  private:
    const DX12RendererContext &m_context;
    DX12DescriptorHeapAllocator &m_heap_allocator;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_handle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle{};
};

} // namespace Horizon::Backend
