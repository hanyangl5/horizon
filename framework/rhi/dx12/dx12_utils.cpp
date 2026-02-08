#include "dx12_utils.h"
#include "DirectXHelpers.h"
#include <core/definations.h>
#include <core/log.h>
namespace Horizon
{

D3D12_COMMAND_LIST_TYPE ToDX12CommandListType(CommandQueueType type) noexcept
{
    switch (type)
    {
    case CommandQueueType::GRAPHICS:
        return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case CommandQueueType::COMPUTE:
        return D3D12_COMMAND_LIST_TYPE_COMPUTE;
    case CommandQueueType::TRANSFER:
        return D3D12_COMMAND_LIST_TYPE_COPY;
    default:
        LOG_ERROR("Invalid command queue type");
        return D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
}

D3D12_RESOURCE_STATES ToDX12ResourceState(ResourceState state) noexcept
{

    // DirectX 12 resource states map directly to our ResourceState enum
    // However, PRESENT state (0x1000) is not a valid D3D12_RESOURCE_STATES value
    // D3D12 uses D3D12_RESOURCE_STATE_COMMON (0) for initial state of committed resources
    if (state == RESOURCE_STATE_PRESENT)
    {
        // PRESENT is a special state for swap chain back buffers, not for regular resources
        // Use COMMON as the initial state for CreateCommittedResource
        return D3D12_RESOURCE_STATE_COMMON;
    }
    if (state == RESOURCE_STATE_UNDEFINED)
    {
        // UNDEFINED is a Vulkan concept, in D3D12 use COMMON
        return D3D12_RESOURCE_STATE_COMMON;
    }
    return static_cast<D3D12_RESOURCE_STATES>(state);
}

D3D12_RESOURCE_FLAGS ToDX12ResourceFlags(DescriptorTypes types) noexcept
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if (types & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (types & DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (types & DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    return flags;
}

D3D12_SRV_DIMENSION ToDX12SRVDimension(TextureType type, bool is_array) noexcept
{
    switch (type)
    {
    case TextureType::TEXTURE_TYPE_1D:
        return is_array ? D3D12_SRV_DIMENSION_TEXTURE1DARRAY : D3D12_SRV_DIMENSION_TEXTURE1D;
    case TextureType::TEXTURE_TYPE_2D:
        return is_array ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D;
    case TextureType::TEXTURE_TYPE_3D:
        return D3D12_SRV_DIMENSION_TEXTURE3D;
    case TextureType::TEXTURE_TYPE_CUBE:
        return is_array ? D3D12_SRV_DIMENSION_TEXTURECUBEARRAY : D3D12_SRV_DIMENSION_TEXTURECUBE;
    default:
        return D3D12_SRV_DIMENSION_UNKNOWN;
    }
}

D3D12_UAV_DIMENSION ToDX12UAVDimension(TextureType type, bool is_array) noexcept
{
    switch (type)
    {
    case TextureType::TEXTURE_TYPE_1D:
        return is_array ? D3D12_UAV_DIMENSION_TEXTURE1DARRAY : D3D12_UAV_DIMENSION_TEXTURE1D;
    case TextureType::TEXTURE_TYPE_2D:
        return is_array ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY : D3D12_UAV_DIMENSION_TEXTURE2D;
    case TextureType::TEXTURE_TYPE_3D:
        return D3D12_UAV_DIMENSION_TEXTURE3D;
    default:
        return D3D12_UAV_DIMENSION_UNKNOWN;
    }
}

D3D12_RTV_DIMENSION ToDX12RTVDimension(TextureType type, bool is_array) noexcept
{
    switch (type)
    {
    case TextureType::TEXTURE_TYPE_1D:
        return is_array ? D3D12_RTV_DIMENSION_TEXTURE1DARRAY : D3D12_RTV_DIMENSION_TEXTURE1D;
    case TextureType::TEXTURE_TYPE_2D:
        return is_array ? D3D12_RTV_DIMENSION_TEXTURE2DARRAY : D3D12_RTV_DIMENSION_TEXTURE2D;
    case TextureType::TEXTURE_TYPE_3D:
        return D3D12_RTV_DIMENSION_TEXTURE3D;
    default:
        return D3D12_RTV_DIMENSION_UNKNOWN;
    }
}

D3D12_DSV_DIMENSION ToDX12DSVDimension(TextureType type, bool is_array) noexcept
{
    switch (type)
    {
    case TextureType::TEXTURE_TYPE_1D:
        return is_array ? D3D12_DSV_DIMENSION_TEXTURE1DARRAY : D3D12_DSV_DIMENSION_TEXTURE1D;
    case TextureType::TEXTURE_TYPE_2D:
        return is_array ? D3D12_DSV_DIMENSION_TEXTURE2DARRAY : D3D12_DSV_DIMENSION_TEXTURE2D;
    default:
        return D3D12_DSV_DIMENSION_UNKNOWN;
    }
}

DXGI_FORMAT ToDX12Format(TextureFormat format) noexcept
{
    switch (format)
    {
    case TextureFormat::TEXTURE_FORMAT_R8_UNORM:
        return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::TEXTURE_FORMAT_RG8_UNORM:
        return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::TEXTURE_FORMAT_R32_SFLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT:
        return DXGI_FORMAT_R11G11B10_FLOAT;
    case TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    default:
        LOG_ERROR("Unsupported texture format for DX12");
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT ToDX12IndexFormat(TextureFormat format) noexcept
{
    // DX12 doesn't use texture format for index buffers, but we keep this for compatibility
    return DXGI_FORMAT_UNKNOWN;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12PrimitiveTopologyType(PrimitiveTopology topology) noexcept
{
    switch (topology)
    {
    case PrimitiveTopology::POINT_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case PrimitiveTopology::LINE_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PrimitiveTopology::TRIANGLE_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

D3D12_PRIMITIVE_TOPOLOGY ToDX12PrimitiveTopology(PrimitiveTopology topology) noexcept
{
    switch (topology)
    {
    case PrimitiveTopology::POINT_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case PrimitiveTopology::LINE_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case PrimitiveTopology::TRIANGLE_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    default:
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

D3D12_CULL_MODE ToDX12CullMode(CullMode cull_mode) noexcept
{
    switch (cull_mode)
    {
    case CullMode::NONE:
        return D3D12_CULL_MODE_NONE;
    case CullMode::FRONT:
        return D3D12_CULL_MODE_FRONT;
    case CullMode::BACK:
        return D3D12_CULL_MODE_BACK;
    default:
        return D3D12_CULL_MODE_NONE;
    }
}

D3D12_FILL_MODE ToDX12FillMode(FillMode fill_mode) noexcept
{
    switch (fill_mode)
    {
    case FillMode::POINT:
        return D3D12_FILL_MODE_WIREFRAME; // DX12 doesn't have point fill mode
    case FillMode::LINE:
        return D3D12_FILL_MODE_WIREFRAME;
    case FillMode::TRIANGLE:
        return D3D12_FILL_MODE_SOLID;
    default:
        return D3D12_FILL_MODE_SOLID;
    }
}

D3D12_COMPARISON_FUNC ToDX12ComparisonFunc(CompareFunc func) noexcept
{
    switch (func)
    {
    case CompareFunc::NEVER:
        return D3D12_COMPARISON_FUNC_NEVER;
    case CompareFunc::LESS:
        return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunc::L_EQUAL:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunc::EQUAL:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunc::GREATER:
        return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunc::G_EQUAL:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunc::ALWAYS:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    default:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

D3D12_FILTER ToDX12Filter(FilterType min_filter, FilterType mag_filter, MipMapMode mip_map_mode,
                          bool comparison) noexcept
{
    bool min_linear = (min_filter == FilterType::FILTER_LINEAR);
    bool mag_linear = (mag_filter == FilterType::FILTER_LINEAR);
    bool mip_linear = (mip_map_mode == MipMapMode::MIPMAP_MODE_LINEAR);

    // Map to D3D12_FILTER enum values
    // D3D12_FILTER is encoded as: (min << 2) | (mag << 4) | (mip << 6) | (comparison ? 0x80 : 0)
    // But we use predefined enum values for simplicity
    if (comparison)
    {
        if (min_linear && mag_linear && mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        else if (min_linear && mag_linear && !mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        else if (min_linear && !mag_linear && mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        else if (min_linear && !mag_linear && !mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
        else if (!min_linear && mag_linear && mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        else if (!min_linear && mag_linear && !mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
        else if (!min_linear && !mag_linear && mip_linear)
            return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        else
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    }
    else
    {
        if (min_linear && mag_linear && mip_linear)
            return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        else if (min_linear && mag_linear && !mip_linear)
            return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        else if (min_linear && !mag_linear && mip_linear)
            return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        else if (min_linear && !mag_linear && !mip_linear)
            return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        else if (!min_linear && mag_linear && mip_linear)
            return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        else if (!min_linear && mag_linear && !mip_linear)
            return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        else if (!min_linear && !mag_linear && mip_linear)
            return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        else
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
}

D3D12_TEXTURE_ADDRESS_MODE ToDX12AddressMode(AddressMode address_mode) noexcept
{
    switch (address_mode)
    {
    case AddressMode::ADDRESS_MODE_MIRROR:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case AddressMode::ADDRESS_MODE_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    default:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12_DESCRIPTOR_RANGE_TYPE ToDX12DescriptorRangeType(DescriptorType type) noexcept
{
    if (type & DESCRIPTOR_TYPE_SAMPLER)
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    if (type & DESCRIPTOR_TYPE_CONSTANT_BUFFER)
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    if (type & DESCRIPTOR_TYPE_TEXTURE)
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    if (type & DESCRIPTOR_TYPE_RW_TEXTURE)
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    if (type & DESCRIPTOR_TYPE_BUFFER || type & DESCRIPTOR_TYPE_RW_BUFFER)
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // Can be SRV or UAV depending on usage

    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
}

D3D12_SHADER_VISIBILITY ToDX12ShaderVisibility(ShaderType type) noexcept
{
    switch (type)
    {
    case ShaderType::VERTEX_SHADER:
        return D3D12_SHADER_VISIBILITY_VERTEX;
    case ShaderType::PIXEL_SHADER:
        return D3D12_SHADER_VISIBILITY_PIXEL;
    case ShaderType::COMPUTE_SHADER:
        return D3D12_SHADER_VISIBILITY_ALL; // Compute shaders use ALL
    default:
        return D3D12_SHADER_VISIBILITY_ALL;
    }
}

DXGI_FORMAT ToDX12VertexFormat(VertexAttribFormat format, u32 portions) noexcept
{
    // TODO: Implement full vertex format conversion
    switch (format)
    {
    case VertexAttribFormat::F32:
        if (portions == 1)
            return DXGI_FORMAT_R32_FLOAT;
        if (portions == 2)
            return DXGI_FORMAT_R32G32_FLOAT;
        if (portions == 3)
            return DXGI_FORMAT_R32G32B32_FLOAT;
        if (portions == 4)
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        break;
    default:
        LOG_ERROR("Unsupported vertex format for DX12");
        return DXGI_FORMAT_UNKNOWN;
    }
    return DXGI_FORMAT_UNKNOWN;
}

const char *GetDX12SemanticName(const VertexAttributeDescription &attr) noexcept
{
    // If semantic_name is explicitly provided, use it
    if (attr.semantic_name != nullptr && attr.semantic_name[0] != '\0')
    {
        return attr.semantic_name;
    }

    // Otherwise, generate default semantic name based on location
    // Common convention: location 0 = POSITION, 1 = NORMAL, 2+ = TEXCOORD
    switch (attr.location)
    {
    case 0:
        return "POSITION";
    case 1:
        return "NORMAL";
    case 2:
        return "TEXCOORD";
    case 3:
        return "TEXCOORD";
    case 4:
        return "TANGENT";
    case 5:
        return "COLOR";
    default:
        // For locations beyond common ones, use TEXCOORD with index
        return "TEXCOORD";
    }
}

u32 GetDX12SemanticIndex(const VertexAttributeDescription &attr) noexcept
{
    // If semantic_name is explicitly provided, use the provided semantic_index
    if (attr.semantic_name != nullptr && attr.semantic_name[0] != '\0')
    {
        return attr.semantic_index;
    }

    // Otherwise, generate default semantic index based on location
    // For TEXCOORD semantics (location >= 2), use location - 2 as index
    if (attr.location >= 2)
    {
        return attr.location - 2;
    }

    // For POSITION, NORMAL, etc., use index 0
    return 0;
}

// Helper function to convert std::string to std::wstring
std::wstring StringToWString(const std::string &str)
{
    if (str.empty())
    {
        return std::wstring();
    }

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
    if (size_needed == 0)
    {
        return std::wstring();
    }

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], size_needed);
    return result;
}

} // namespace Horizon
