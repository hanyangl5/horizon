#include "dx12_buffer.h"
#include <DirectXHelpers.h>
#include <core/log.h>
#include <core/memory.h>

namespace Horizon::Backend
{

DX12Buffer::DX12Buffer(const DX12RendererContext &context, const BufferCreateInfo &buffer_create_info) noexcept
    : Buffer(buffer_create_info), m_context(context),
      m_current_state(ToDX12ResourceState(buffer_create_info.initial_state))
{

    D3D12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_create_info.size);

    // Determine resource flags based on descriptor types
    if (buffer_create_info.descriptor_types & DESCRIPTOR_TYPE_RW_BUFFER)
    {
        resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    HRESULT hr =
        m_context.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                                  D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DX12 buffer: {}", hr);
        return;
    }

    if (buffer_create_info.debug_name)
    {
        m_resource->SetName(std::wstring(buffer_create_info.debug_name,
                                         buffer_create_info.debug_name + strlen(buffer_create_info.debug_name))
                                .c_str());
    }
}

DX12Buffer::~DX12Buffer() noexcept
{
    // Microsoft::WRL::ComPtr will automatically release the resources (m_resource and m_upload_buffer)
}

ID3D12Resource *DX12Buffer::GetUploadBuffer() noexcept
{
    // Create upload buffer lazily if it doesn't exist
    if (m_upload_buffer == nullptr)
    {
        CD3DX12_HEAP_PROPERTIES upload_heap_props(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(m_size);

        HRESULT hr = m_context.device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE,
                                                               &upload_buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                               nullptr, IID_PPV_ARGS(&m_upload_buffer));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create upload buffer: {}", hr);
            return nullptr;
        }

        // Set debug name based on the main buffer's name
        std::wstring upload_name = L"UploadBuffer_";
        m_upload_buffer->SetName(upload_name.c_str());
    }

    return m_upload_buffer.Get();
}

} // namespace Horizon::Backend
