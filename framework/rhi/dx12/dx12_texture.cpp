#include "dx12_texture.h"
#include <DirectXHelpers.h>
#include <core/log.h>
#include <core/memory.h>

namespace Horizon::Backend
{

DX12Texture::DX12Texture(const DX12RendererContext &context, const TextureCreateInfo &texture_create_info) noexcept
    : Texture(texture_create_info), m_context(context),
      m_current_state(ToDX12ResourceState(texture_create_info.initial_state))
{
    // rt tex
    if (texture_create_info.texture_format == TextureFormat::TEXTURE_FORMAT_UNDEFINED ||
        texture_create_info.texture_format == TextureFormat::TEXTURE_FORMAT_DUMMY_COLOR)
        return;
    CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DIMENSION dimension =
        (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_3D)   ? D3D12_RESOURCE_DIMENSION_TEXTURE3D
        : (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_1D) ? D3D12_RESOURCE_DIMENSION_TEXTURE1D
                                                                             : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    UINT16 depth_or_array_size = (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_3D)
                                     ? static_cast<UINT16>(texture_create_info.depth)
                                     : static_cast<UINT16>(texture_create_info.array_layer);

    CD3DX12_RESOURCE_DESC resource_desc(
        dimension, 0, texture_create_info.width, texture_create_info.height, depth_or_array_size,
        1, // MipLevels - must be >= 1 for CreateCommittedResource
        ToDX12Format(texture_create_info.texture_format), 1, 0, D3D12_TEXTURE_LAYOUT_UNKNOWN,
        ToDX12ResourceFlags(texture_create_info.descriptor_types));

    // Validate format
    if (resource_desc.Format == DXGI_FORMAT_UNKNOWN)
    {
        LOG_ERROR("Invalid texture format for DX12 texture creation");
        return;
    }

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
    // Microsoft::WRL::ComPtr will automatically release the resource
}

} // namespace Horizon::Backend
