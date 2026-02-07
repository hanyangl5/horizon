#include "dx12_texture.h"
#include <core/log.h>
#include <core/memory.h>

namespace Horizon::Backend
{

DX12Texture::DX12Texture(const DX12RendererContext &context, const TextureCreateInfo &texture_create_info) noexcept
    : Texture(texture_create_info), m_context(context),
      m_current_state(ToDX12ResourceState(texture_create_info.initial_state))
{
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension =
        (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_3D)   ? D3D12_RESOURCE_DIMENSION_TEXTURE3D
        : (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_1D) ? D3D12_RESOURCE_DIMENSION_TEXTURE1D
                                                                             : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Alignment = 0;
    resource_desc.Width = texture_create_info.width;
    resource_desc.Height = texture_create_info.height;
    resource_desc.DepthOrArraySize = (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_3D)
                                         ? texture_create_info.depth
                                         : texture_create_info.array_layer;
    resource_desc.MipLevels = texture_create_info.enanble_mipmap ? 0 : 1; // 0 means all mip levels
    resource_desc.Format = ToDX12Format(texture_create_info.texture_format);
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = ToDX12ResourceFlags(texture_create_info.descriptor_types);

    HRESULT hr = m_context.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                                           m_current_state, nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DX12 texture: {}", hr);
        return;
    }

    if (texture_create_info.debug_name)
    {
        m_resource->SetName(std::wstring(texture_create_info.debug_name,
                                         texture_create_info.debug_name + strlen(texture_create_info.debug_name))
                                .c_str());
    }

    // TODO: Create SRV and UAV descriptors if needed
}

DX12Texture::~DX12Texture() noexcept
{
    // ComPtr will automatically release the resource
}

} // namespace Horizon::Backend
