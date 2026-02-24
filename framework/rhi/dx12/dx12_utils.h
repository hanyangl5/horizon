#pragma once

#include <string>

#include "d3dx12.h"
#include <DirectXHelpers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/enums.h>

namespace Horizon
{

struct DX12RendererContext
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queues[3]; // Graphics, Compute, Transfer
    Microsoft::WRL::ComPtr<ID3D12Fence> fences[3];
    UINT64 fence_values[3];
    HANDLE fence_event;
    UINT rtv_descriptor_size;
    UINT dsv_descriptor_size;
    UINT srv_uav_descriptor_size;
    UINT cbv_descriptor_size;
    UINT sampler_descriptor_size;

    // Command signatures for indirect drawing
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> draw_indexed_indirect_command_signature;
};

D3D12_COMMAND_LIST_TYPE ToDX12CommandListType(CommandQueueType type) noexcept;

D3D12_RESOURCE_STATES ToDX12ResourceState(ResourceState state) noexcept;

D3D12_RESOURCE_FLAGS ToDX12ResourceFlags(DescriptorTypes types) noexcept;

D3D12_SRV_DIMENSION ToDX12SRVDimension(TextureType type, bool is_array) noexcept;

D3D12_UAV_DIMENSION ToDX12UAVDimension(TextureType type, bool is_array) noexcept;

D3D12_RTV_DIMENSION ToDX12RTVDimension(TextureType type, bool is_array) noexcept;

D3D12_DSV_DIMENSION ToDX12DSVDimension(TextureType type, bool is_array) noexcept;

DXGI_FORMAT ToDX12Format(TextureFormat format) noexcept;

DXGI_FORMAT ToDX12IndexFormat(TextureFormat format) noexcept;

D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12PrimitiveTopologyType(PrimitiveTopology topology) noexcept;

D3D12_PRIMITIVE_TOPOLOGY ToDX12PrimitiveTopology(PrimitiveTopology topology) noexcept;

D3D12_CULL_MODE ToDX12CullMode(CullMode cull_mode) noexcept;

D3D12_FILL_MODE ToDX12FillMode(FillMode fill_mode) noexcept;

D3D12_COMPARISON_FUNC ToDX12ComparisonFunc(CompareFunc func) noexcept;

D3D12_FILTER ToDX12Filter(FilterType min_filter, FilterType mag_filter, MipMapMode mip_map_mode,
                          bool comparison) noexcept;

D3D12_TEXTURE_ADDRESS_MODE ToDX12AddressMode(AddressMode address_mode) noexcept;

D3D12_DESCRIPTOR_RANGE_TYPE ToDX12DescriptorRangeType(DescriptorType type) noexcept;

D3D12_SHADER_VISIBILITY ToDX12ShaderVisibility(ShaderType type) noexcept;

DXGI_FORMAT ToDX12VertexFormat(VertexAttribFormat format, u32 portions) noexcept;

// Get semantic name for DX12 vertex attribute
// If semantic_name is provided, use it; otherwise generate from location
const char *GetDX12SemanticName(const VertexAttributeDescription &attr) noexcept;
u32 GetDX12SemanticIndex(const VertexAttributeDescription &attr) noexcept;

std::wstring StringToWString(const std::string &str);
std::string WStringToString(const std::wstring &wstr);
} // namespace Horizon
