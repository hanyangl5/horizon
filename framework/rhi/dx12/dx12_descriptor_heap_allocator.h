#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace Horizon::Backend
{

class DX12DescriptorHeapAllocator
{
  public:
    DX12DescriptorHeapAllocator(const DX12RendererContext &context) noexcept;
    ~DX12DescriptorHeapAllocator() noexcept;

    DX12DescriptorHeapAllocator(const DX12DescriptorHeapAllocator &rhs) noexcept = delete;
    DX12DescriptorHeapAllocator &operator=(const DX12DescriptorHeapAllocator &rhs) noexcept = delete;
    DX12DescriptorHeapAllocator(DX12DescriptorHeapAllocator &&rhs) noexcept = delete;
    DX12DescriptorHeapAllocator &operator=(DX12DescriptorHeapAllocator &&rhs) noexcept = delete;

    void ResetDescriptorHeaps();

    // Allocate descriptors
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSV();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRV();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateUAV();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCBV();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSampler();

    // Allocate multiple descriptors for bindless resources
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRVs(u32 count);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateUAVs(u32 count);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCBVs(u32 count);

    // Get descriptor heap
    ID3D12DescriptorHeap *GetSRVUAVCBVHeap() const
    {
        return m_srv_uav_cbv_heap.Get();
    }
    ID3D12DescriptorHeap *GetRTVHeap() const
    {
        return m_rtv_heap.Get();
    }
    ID3D12DescriptorHeap *GetDSVHeap() const
    {
        return m_dsv_heap.Get();
    }
    ID3D12DescriptorHeap *GetSamplerHeap() const
    {
        return m_sampler_heap.Get();
    }

  private:
    void CreateDescriptorHeaps();

    const DX12RendererContext &m_context;

    // Descriptor heaps
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    ComPtr<ID3D12DescriptorHeap> m_dsv_heap;
    ComPtr<ID3D12DescriptorHeap> m_srv_uav_cbv_heap;
    ComPtr<ID3D12DescriptorHeap> m_sampler_heap;

    // Allocation counters
    static constexpr u32 MAX_RTV_COUNT = 1024;
    static constexpr u32 MAX_DSV_COUNT = 256;
    static constexpr u32 MAX_SRV_UAV_CBV_COUNT = 8192;
    static constexpr u32 MAX_SAMPLER_COUNT = 256;

    u32 m_rtv_index{0};
    u32 m_dsv_index{0};
    u32 m_srv_uav_cbv_index{0};
    u32 m_sampler_index{0};
};

} // namespace Horizon::Backend
