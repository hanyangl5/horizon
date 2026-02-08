#include "dx12_sampler.h"
#include "dx12_descriptor_heap_allocator.h"
#include <core/log.h>

namespace Horizon::Backend
{

DX12Sampler::DX12Sampler(const DX12RendererContext &context, DX12DescriptorHeapAllocator &heap_allocator,
                         const SamplerDesc &desc) noexcept
    : m_context(context), m_heap_allocator(heap_allocator)
{
    // Allocate sampler descriptor
    m_cpu_handle = m_heap_allocator.AllocateSampler();

    // Calculate GPU handle
    auto heap_start_cpu = m_heap_allocator.GetSamplerHeap()->GetCPUDescriptorHandleForHeapStart();
    auto heap_start_gpu = m_heap_allocator.GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart();
    SIZE_T offset = m_cpu_handle.ptr - heap_start_cpu.ptr;
    m_gpu_handle.ptr = heap_start_gpu.ptr + offset;

    // Create sampler state description
    D3D12_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = Horizon::ToDX12Filter(desc.min_filter, desc.mag_filter, desc.mip_map_mode,
                                                desc.mCompareFunc != CompareFunc::NEVER);
    sampler_desc.AddressU = Horizon::ToDX12AddressMode(desc.address_u);
    sampler_desc.AddressV = Horizon::ToDX12AddressMode(desc.address_v);
    sampler_desc.AddressW = Horizon::ToDX12AddressMode(desc.address_w);
    sampler_desc.MipLODBias = desc.mMipLodBias;
    sampler_desc.MaxAnisotropy = static_cast<UINT>(desc.mMaxAnisotropy);
    sampler_desc.ComparisonFunc = Horizon::ToDX12ComparisonFunc(desc.mCompareFunc);

    // Set border color (DX12 doesn't have separate border color types, use transparent black)
    sampler_desc.BorderColor[0] = 0.0f;
    sampler_desc.BorderColor[1] = 0.0f;
    sampler_desc.BorderColor[2] = 0.0f;
    sampler_desc.BorderColor[3] = 0.0f;

    // Set LOD range
    if (desc.mSetLodRange)
    {
        sampler_desc.MinLOD = desc.mMinLod;
        sampler_desc.MaxLOD = desc.mMaxLod;
    }
    else
    {
        sampler_desc.MinLOD = 0.0f;
        sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    // Create sampler
    m_context.device->CreateSampler(&sampler_desc, m_cpu_handle);
}

DX12Sampler::~DX12Sampler() noexcept
{
    // Descriptor will be freed when heap is reset
}

} // namespace Horizon::Backend
