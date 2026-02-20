#include "dx12_descriptor_heap_allocator.h"
#include <DirectXHelpers.h>
#include <core/log.h>

namespace Horizon::Backend
{

DX12DescriptorHeapAllocator::DX12DescriptorHeapAllocator(const DX12RendererContext &context) noexcept
    : m_context(context)
{
    CreateDescriptorHeaps();
}

DX12DescriptorHeapAllocator::~DX12DescriptorHeapAllocator() noexcept
{
    // Microsoft::WRL::ComPtr will automatically release
}

void DX12DescriptorHeapAllocator::CreateDescriptorHeaps()
{
    // Create RTV heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors = MAX_RTV_COUNT;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = m_context.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_rtv_heap));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create RTV descriptor heap: {}", hr);
        }
    }

    // Create DSV heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors = MAX_DSV_COUNT;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = m_context.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_dsv_heap));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create DSV descriptor heap: {}", hr);
        }
    }

    // Create SRV/UAV/CBV heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors = MAX_SRV_UAV_CBV_COUNT;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HRESULT hr = m_context.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_srv_uav_cbv_heap));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create SRV/UAV/CBV descriptor heap: {}", hr);
        }
    }

    // Create Sampler heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors = MAX_SAMPLER_COUNT;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HRESULT hr = m_context.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_sampler_heap));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create Sampler descriptor heap: {}", hr);
        }
    }

    // Non-shader-visible SRV/UAV/CBV heap (required as CPU handle for ClearUnorderedAccessView*)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors = MAX_STAGING_SRV_UAV_CBV_COUNT;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = m_context.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_staging_srv_uav_cbv_heap));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create staging SRV/UAV/CBV descriptor heap: {}", hr);
        }
    }
}

void DX12DescriptorHeapAllocator::ResetDescriptorHeaps()
{
    m_rtv_index = 0;
    m_dsv_index = 0;
    m_srv_uav_cbv_index = 0;
    m_sampler_index = 0;
    m_staging_srv_uav_cbv_index = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateRTV()
{
    if (m_rtv_index >= MAX_RTV_COUNT)
    {
        LOG_ERROR("RTV descriptor heap exhausted");
        return {};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_rtv_index * m_context.rtv_descriptor_size;
    m_rtv_index++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateDSV()
{
    if (m_dsv_index >= MAX_DSV_COUNT)
    {
        LOG_ERROR("DSV descriptor heap exhausted");
        return {};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_dsv_index * m_context.dsv_descriptor_size;
    m_dsv_index++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateSRV()
{
    if (m_srv_uav_cbv_index >= MAX_SRV_UAV_CBV_COUNT)
    {
        LOG_ERROR("SRV/UAV/CBV descriptor heap exhausted");
        return {};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srv_uav_cbv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_srv_uav_cbv_index * m_context.srv_uav_descriptor_size;
    m_srv_uav_cbv_index++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateUAV()
{
    // UAV uses the same heap as SRV
    return AllocateSRV();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateCBV()
{
    // CBV uses the same heap as SRV
    return AllocateSRV();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateSampler()
{
    if (m_sampler_index >= MAX_SAMPLER_COUNT)
    {
        LOG_ERROR("Sampler descriptor heap exhausted");
        return {};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_sampler_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_sampler_index * m_context.sampler_descriptor_size;
    m_sampler_index++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateSRVs(u32 count)
{
    if (m_srv_uav_cbv_index + count > MAX_SRV_UAV_CBV_COUNT)
    {
        LOG_ERROR("SRV/UAV/CBV descriptor heap exhausted (need {}, have {})", count,
                  MAX_SRV_UAV_CBV_COUNT - m_srv_uav_cbv_index);
        return {};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srv_uav_cbv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_srv_uav_cbv_index * m_context.srv_uav_descriptor_size;
    m_srv_uav_cbv_index += count;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateUAVs(u32 count)
{
    // UAV uses the same heap as SRV
    return AllocateSRVs(count);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateCBVs(u32 count)
{
    // CBV uses the same heap as SRV
    return AllocateSRVs(count);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateStagingSRV()
{
    if (m_staging_srv_uav_cbv_index >= MAX_STAGING_SRV_UAV_CBV_COUNT)
    {
        LOG_ERROR("Staging SRV/UAV/CBV descriptor heap exhausted");
        return {};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_staging_srv_uav_cbv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_staging_srv_uav_cbv_index * m_context.srv_uav_descriptor_size;
    m_staging_srv_uav_cbv_index++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::AllocateStagingUAV()
{
    return AllocateStagingSRV();
}

} // namespace Horizon::Backend
