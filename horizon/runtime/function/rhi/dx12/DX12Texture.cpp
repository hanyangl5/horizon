#include <runtime/function/rhi/dx12/DX12Texture.h>

namespace Horizon::RHI {

DX12Texture::DX12Texture(D3D12MA::Allocator *allocator,
                         const TextureCreateInfo &texture_create_info) noexcept
    : Texture(texture_create_info), m_allocator(allocator) {
    // Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0,
    // which is effectively 64KB.

    CD3DX12_RESOURCE_DESC _texture_create_info{};
    _texture_create_info.Dimension =
        ToDX12TextureDimension(texture_create_info.texture_type);
    _texture_create_info.Format =
        ToDx12TextureFormat(texture_create_info.texture_format);
    _texture_create_info.Flags =
        ToDX12TextureUsage(texture_create_info.texture_usage);
    _texture_create_info.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    _texture_create_info.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    _texture_create_info.Width = texture_create_info.width;
    _texture_create_info.Height = texture_create_info.height;
    _texture_create_info.DepthOrArraySize = texture_create_info.depth;
    _texture_create_info.MipLevels = 1;
    _texture_create_info.SampleDesc.Count = 1;
    _texture_create_info.SampleDesc.Quality = 0;

    D3D12MA::ALLOCATION_DESC allocation_desc = {};
    allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    CHECK_DX_RESULT(allocator->CreateResource(
        &allocation_desc, &_texture_create_info, D3D12_RESOURCE_STATE_COPY_DEST,
        NULL, &m_allocation, IID_NULL, NULL));
}

DX12Texture::~DX12Texture() noexcept { Destroy(); }

void DX12Texture::Destroy() noexcept { m_allocation->Release(); }
} // namespace Horizon::RHI
