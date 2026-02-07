#include "dx12_buffer.h"
#include <core/memory.h>
#include <core/log.h>

namespace Horizon::Backend
{

DX12Buffer::DX12Buffer(const DX12RendererContext &context, const BufferCreateInfo &buffer_create_info) noexcept
    : Buffer(buffer_create_info), m_context(context), m_current_state(ToDX12ResourceState(buffer_create_info.initial_state))
{
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = buffer_create_info.size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Determine resource flags based on descriptor types
    if (buffer_create_info.descriptor_types & DESCRIPTOR_TYPE_RW_BUFFER)
    {
        resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    HRESULT hr = m_context.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                                           m_current_state, nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DX12 buffer: {}", hr);
        return;
    }

    if (buffer_create_info.debug_name)
    {
        m_resource->SetName(std::wstring(buffer_create_info.debug_name, buffer_create_info.debug_name + strlen(buffer_create_info.debug_name)).c_str());
    }
}

DX12Buffer::~DX12Buffer() noexcept
{
    // ComPtr will automatically release the resource
}

} // namespace Horizon::Backend
