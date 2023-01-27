#pragma once


#include <fstream>

//#include "dx12/stdafx.h"
//#include <d3d12.h>

#include <runtime/core/log/log.h>
#include <runtime/core/math/math.h>
#include <runtime/core/container/Container.h>
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/vulkan/vulkan_config.h>

namespace Horizon {

// definations

// descriptor set
static constexpr u32 DESCRIPTOR_SET_UPDATE_FREQUENCIES = 5;

//static constexpr u32 MAX_BINDING_PER_DESCRIPTOR_SET = 32;

// render info
static constexpr u32 MAX_RENDER_TARGET_COUNT = 8;

// vertex input
static constexpr u32 MAX_ATTRIBUTE_COUNT = 32;
static constexpr u32 MAX_BINDING_COUNT = 32;

enum class RenderBackend { RENDER_BACKEND_NONE, RENDER_BACKEND_VULKAN, RENDER_BACKEND_DX12 };
// always assum queue family index: graphics = 0, compute = 1, transfer = 2
enum CommandQueueType { GRAPHICS = 0, COMPUTE, TRANSFER };

enum class PipelineType { UNDIFINED, GRAPHICS = 0, COMPUTE, RAY_TRACING };

using DescriptorTypes = u32;

enum DescriptorType {
    DESCRIPTOR_TYPE_UNDEFINED = 0,
    DESCRIPTOR_TYPE_SAMPLER = 0x01,
    // SRV Read only texture
    DESCRIPTOR_TYPE_TEXTURE = (DESCRIPTOR_TYPE_SAMPLER << 1),
    /// UAV Texture
    DESCRIPTOR_TYPE_RW_TEXTURE = (DESCRIPTOR_TYPE_TEXTURE << 1),
    // SRV Read only buffer
    DESCRIPTOR_TYPE_BUFFER = (DESCRIPTOR_TYPE_RW_TEXTURE << 1),
    DESCRIPTOR_TYPE_BUFFER_RAW = (DESCRIPTOR_TYPE_BUFFER | (DESCRIPTOR_TYPE_BUFFER << 1)),
    /// UAV Buffer
    DESCRIPTOR_TYPE_RW_BUFFER = (DESCRIPTOR_TYPE_BUFFER << 2),
    DESCRIPTOR_TYPE_RW_BUFFER_RAW = (DESCRIPTOR_TYPE_RW_BUFFER | (DESCRIPTOR_TYPE_RW_BUFFER << 1)),
    /// Uniform buffer
    DESCRIPTOR_TYPE_CONSTANT_BUFFER = (DESCRIPTOR_TYPE_RW_BUFFER << 2),
    /// Push constant / Root constant
    DESCRIPTOR_TYPE_ROOT_CONSTANT = (DESCRIPTOR_TYPE_CONSTANT_BUFFER << 1),
    /// IA
    DESCRIPTOR_TYPE_VERTEX_BUFFER = (DESCRIPTOR_TYPE_ROOT_CONSTANT << 1),
    DESCRIPTOR_TYPE_INDEX_BUFFER = (DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
    DESCRIPTOR_TYPE_INDIRECT_BUFFER = (DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
    /// Cubemap SRV
    DESCRIPTOR_TYPE_TEXTURE_CUBE = (DESCRIPTOR_TYPE_TEXTURE | (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1)),
    /// RTV / DSV per mip slice
    DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES = (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 2),
    /// RTV / DSV per array slice
    DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES << 1),
    /// RTV / DSV per depth slice
    DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
    DESCRIPTOR_TYPE_RAY_TRACING = (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
#if defined(USE_VULKAN)
    /// Subpass input (descriptor type only available in Vulkan)
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (DESCRIPTOR_TYPE_RAY_TRACING << 1),
    DESCRIPTOR_TYPE_TEXEL_BUFFER = (DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
    DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = (DESCRIPTOR_TYPE_RW_TEXEL_BUFFER << 1),

    /// Khronos extension ray tracing
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE = (DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER << 1),
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT = (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE << 1),
    DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS = (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT << 1),
    DESCRIPTOR_TYPE_SHADER_BINDING_TABLE = (DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS << 1),
#endif
    DESCRIPTOR_TYPE_COLOR_ATTACHMENT = (DESCRIPTOR_TYPE_RAY_TRACING << 1),
    DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT = (DESCRIPTOR_TYPE_COLOR_ATTACHMENT << 1),
};

enum class ShaderType {
    VERTEX_SHADER,
    PIXEL_SHADER,
    COMPUTE_SHADER,
    // GEOMETRY_SHADER,
    //  ray tracing related shader
};

enum ShaderStageFlags {
    SHADER_STAGE_INVALID = 0,
    SHADER_STAGE_VERTEX_SHADER = 1,
    SHADER_STAGE_PIXEL_SHADER = 2,
    SHADER_STAGE_COMPUTE_SHADER = 4,
    //SHADER_STAGE_TESS_SHADER = 8,
};

enum class TextureType {
    TEXTURE_TYPE_1D = 0,
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_3D,
    TEXTURE_TYPE_CUBE
};

enum class TextureFormat {

    TEXTURE_FORMAT_UNDEFINED = 0,
    TEXTURE_FORMAT_DUMMY_COLOR,
    // unsigned int
    TEXTURE_FORMAT_R8_UINT,
    TEXTURE_FORMAT_RG8_UINT,
    TEXTURE_FORMAT_RGB8_UINT,
    TEXTURE_FORMAT_RGBA8_UINT,

    TEXTURE_FORMAT_R16_UINT,
    TEXTURE_FORMAT_RG16_UINT,
    TEXTURE_FORMAT_RGB16_UINT,
    TEXTURE_FORMAT_RGBA16_UINT,

    TEXTURE_FORMAT_R32_UINT,
    TEXTURE_FORMAT_RG32_UINT,
    TEXTURE_FORMAT_RGB32_UINT,
    TEXTURE_FORMAT_RGBA32_UINT,

    // signed int
    TEXTURE_FORMAT_R8_SINT,
    TEXTURE_FORMAT_RG8_SINT,
    TEXTURE_FORMAT_RGB8_SINT,
    TEXTURE_FORMAT_RGBA8_SINT,

    TEXTURE_FORMAT_R16_SINT,
    TEXTURE_FORMAT_RG16_SINT,
    TEXTURE_FORMAT_RGB16_SINT,
    TEXTURE_FORMAT_RGBA16_SINT,

    TEXTURE_FORMAT_R32_SINT,
    TEXTURE_FORMAT_RG32_SINT,
    TEXTURE_FORMAT_RGB32_SINT,
    TEXTURE_FORMAT_RGBA32_SINT,

    // normalized unsinged int
    TEXTURE_FORMAT_R8_UNORM,
    TEXTURE_FORMAT_RG8_UNORM,
    TEXTURE_FORMAT_RGB8_UNORM,
    TEXTURE_FORMAT_RGBA8_UNORM,

    TEXTURE_FORMAT_R16_UNORM,
    TEXTURE_FORMAT_RG16_UNORM,
    TEXTURE_FORMAT_RGB16_UNORM,
    TEXTURE_FORMAT_RGBA16_UNORM,

    // normalized signed int
    TEXTURE_FORMAT_R8_SNORM,
    TEXTURE_FORMAT_RG8_SNORM,
    TEXTURE_FORMAT_RGB8_SNORM,
    TEXTURE_FORMAT_RGBA8_SNORM,

    TEXTURE_FORMAT_R16_SNORM,
    TEXTURE_FORMAT_RG16_SNORM,
    TEXTURE_FORMAT_RGB16_SNORM,
    TEXTURE_FORMAT_RGBA16_SNORM,

    // signed float
    TEXTURE_FORMAT_R16_SFLOAT,
    TEXTURE_FORMAT_RG16_SFLOAT,
    TEXTURE_FORMAT_RGB16_SFLOAT,
    TEXTURE_FORMAT_RGBA16_SFLOAT,

    TEXTURE_FORMAT_R32_SFLOAT,
    TEXTURE_FORMAT_RG32_SFLOAT,
    TEXTURE_FORMAT_RGB32_SFLOAT,
    TEXTURE_FORMAT_RGBA32_SFLOAT,

    TEXTURE_FORMAT_R10G10B10A2_UNORM,
    TEXTURE_FORMAT_R11G11B10_UNORM,
    TEXTURE_FORMAT_R10G10B10A2_SNORM,
    TEXTURE_FORMAT_R11G11B10_SFLOAT,
    TEXTURE_FORMAT_R11G11B10_UFLOAT,

    TEXTURE_FORMAT_D32_SFLOAT,

};

enum ResourceState {
    RESOURCE_STATE_UNDEFINED = 0,
    RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    RESOURCE_STATE_INDEX_BUFFER = 0x2,
    RESOURCE_STATE_RENDER_TARGET = 0x4,
    RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
    RESOURCE_STATE_DEPTH_WRITE = 0x10,
    RESOURCE_STATE_DEPTH_READ = 0x20,
    RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
    RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
    RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
    RESOURCE_STATE_STREAM_OUT = 0x100,
    RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
    RESOURCE_STATE_COPY_DEST = 0x400,
    RESOURCE_STATE_COPY_SOURCE = 0x800,
    RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
    RESOURCE_STATE_PRESENT = 0x1000,
    RESOURCE_STATE_COMMON = 0x2000,
    RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x4000,
    RESOURCE_STATE_SHADING_RATE_SOURCE = 0x8000,
    RESOURCE_STATE_HOST_READ = 0x10000,
    RESOURCE_STATE_HOST_WRITE = 0x20000 
};

enum class MemoryFlag { DEDICATE_GPU_MEMORY, CPU_VISABLE_MEMORY };

struct BufferCreateInfo {
    // u32 buffer_usage_flags;
    DescriptorTypes descriptor_types;
    ResourceState initial_state;
    u64 size;
};

struct TextureCreateInfo {
    DescriptorTypes descriptor_types{};
    ResourceState initial_state;
    TextureType texture_type;
    TextureFormat texture_format;
    // TextureUsage texture_usage;
    u32 width, height, depth = 1;
    bool enanble_mipmap = false;
    u32 array_layer = 1;
};

using SwapChainFormat = TextureFormat;

struct SwapChainCreateInfo {
    u32 back_buffer_count;
};

// dx12

struct ViewportCreateInfo {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    f32 min_depth;
    f32 max_depth;
};

//// vertex input state vk
// struct VertexInputState {
//     Container::Array<VkVertexInputBindingDescription> binding_descriptions;
//     Container::Array<VkVertexInputAttributeDescription> attribute_descriptions;
// };
//
// struct VertexInputStateDx12 {
//     Container::Array<D3D12_INPUT_ELEMENT_DESC> input_element_descs;
//     D3D12_INPUT_LAYOUT_DESC input_layout_desc;
// };

// struct RasterizationState {
//     VkPolygonMode polygon_mode;
//     VkCullModeFlags cull_mode;
//     VkFrontFace front_face;
//     VkBool32 depth_clamp;
//     VkBool32 rasterizer_discard_enable;
//     f32 depth_bias_constant_factor;
//     f32 depth_bias_clamp;
//     f32 depth_bias_slope_factor;
//     f32 line_width;
// };

enum class VertexInputRate {
    VERTEX_ATTRIB_RATE_VERTEX = 0,
    VERTEX_ATTRIB_RATE_INSTANCE = 1,
};

enum class VertexAttribFormat { U8, U16, U32, S8, S16, S32, F16, F32, UN8, UN16, SN8, SN16 };

struct VertexAttributeDescription {
    VertexAttribFormat attrib_format;
    u32 portion;
    VertexInputRate input_rate;
    u32 location;
    u32 binding;
    u32 stride;
    u32 offset;
};

struct VertexInputState {
    u32 attribute_count;
    VertexAttributeDescription attributes[MAX_ATTRIBUTE_COUNT];
};

enum class PrimitiveTopology { POINT_LIST, LINE_LIST, TRIANGLE_LIST };

struct InputAssemblyState {
    PrimitiveTopology topology;
};

struct ViewPortState {
    u32 width;
    u32 height;
};

enum class FrontFace { CCW, CW };

enum class FillMode { POINT, LINE, TRIANGLE };

enum class CullMode { NONE, FRONT, BACK, ALL };

struct RasterizationState {
    FrontFace front_face;
    CullMode cull_mode;
    FillMode fill_mode;
    bool discard;
};

enum class CompareFunc { NEVER, LESS, L_EQUAL, EQUAL, GREATER, G_EQUAL, ALWAYS };

using DepthFunc = CompareFunc;

struct DepthStencilState {
    bool depth_test;
    bool depth_write;
    bool stencil_enabled;
    DepthFunc depth_func;
    TextureFormat depth_stencil_format;
    f32 depthNear, depthFar;
    // stencil settings
};

struct MultiSampleState {
    u32 sample_count;
};

struct RenderTargetFormats {
    u32 color_attachment_count = 0;
    Container::Array<TextureFormat> color_attachment_formats;
    bool has_depth = true, has_stencil = false;
    TextureFormat depth_stencil_format;
};

struct GraphicsPipelineCreateInfo {
    VertexInputState vertex_input_state;
    InputAssemblyState input_assembly_state;
    ViewPortState view_port_state;
    RasterizationState rasterization_state;
    DepthStencilState depth_stencil_state;
    MultiSampleState multi_sample_state;
    RenderTargetFormats render_target_formats;
};

struct ComputePipelineCreateInfo {
    u32 flag = 0x01;
};

struct PipelineCreateInfo {
    PipelineType type;
    GraphicsPipelineCreateInfo *gpci;
    ComputePipelineCreateInfo *cpci;
};

struct Rect {
    u32 x, y, w, h;
};

using RenderTargetFormat = TextureFormat;

enum class RenderTargetType { COLOR, DEPTH_STENCIL, UNDEFINED };

struct RenderTargetCreateInfo {
    RenderTargetFormat rt_format;
    RenderTargetType rt_type;
    u32 width, height;
};

struct DrawParam {
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    i32 vertexOffset;
    u32 firstInstance;
};

enum class ResourceUpdateFrequency { NONE, PER_FRAME, PER_BATCH, PER_DRAW, BINDLESS, USER_DEFINED1 };

struct DescriptorDesc {
    DescriptorType type{};
    u32 vk_binding{};
    Container::String dx_reg{}; // todo : type -> reg type
};

struct PushConstantDesc {
    u32 size;
    u32 offset;
    u32 shader_stages;
};

struct RootSignatureDesc {
    Container::FixedArray<Container::HashMap<Container::String, DescriptorDesc>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> descriptors{};
    Container::HashMap<Container::String, PushConstantDesc> push_constants;
};


u32 GetStrideFromVertexAttributeDescription(VertexAttribFormat format, u32 portions);

struct VkPipelineLayoutDesc {
  public:
    Container::FixedArray<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES> descriptor_set_hash_key{};
};

inline ShaderStageFlags GetShaderStageFlagsFromShaderType(ShaderType type) {
    switch (type) {
    case Horizon::ShaderType::VERTEX_SHADER:
        return ShaderStageFlags::SHADER_STAGE_VERTEX_SHADER;
    case Horizon::ShaderType::PIXEL_SHADER:
        return ShaderStageFlags::SHADER_STAGE_PIXEL_SHADER;
    case Horizon::ShaderType::COMPUTE_SHADER:
        return ShaderStageFlags::SHADER_STAGE_COMPUTE_SHADER;
    default:
        LOG_ERROR("invalid shader type");
        return ShaderStageFlags::SHADER_STAGE_INVALID;
    }
}

typedef union ClearColorValue {
    Container::FixedArray<f32, 4> float32;
    Container::FixedArray<i32, 4> int32;
    Container::FixedArray<u32, 4> uint32;
} ClearColorValue;

struct ClearValueDepthStencil {
    f32 depth;
    u32 stencil;
};


//union ClearValue {
//    f32 valuef;
//    u32 valueu;
//    i32 valuei;
//};

struct BufferClearInfo {
    u64 offset;
    u64 size;
    u32 clear_value;
};

struct TextureClearInfo {
    u32 first_mip_level{};
    u32 mip_level_count{1};
    u32 first_layer{};
    u32 layer_count{1};
    ClearColorValue clear_value;
};

struct TextureDataDesc {
    u32 width;
    u32 height;
    u32 depth;
    u32 layer_count = 1;
    u32 mipmap_count = 1;
    TextureFormat format;
    TextureType type;
    Container::Array<char> raw_data;
    // we don't have a unified runtime format for mipmap/layer, so we have to store that
    Container::Array<Container::Array<u32>> data_offset_map;
};

struct BufferUpdateDesc {
    void *data;
    u64 size;
    u64 offset;
};

struct TextureUpdateDesc {
    u64 size = 0;
    //u32 row_length;
    //u32 height;
    u32 first_mip_level = 0;
    u32 mip_level_count = 1;
    u32 first_layer = 0;
    u32 layer_count = 1;
    TextureDataDesc *texture_data_desc{};
};

enum class MipMapMode { MIPMAP_MODE_NEAREST = 0, MIPMAP_MODE_LINEAR };

enum class FilterType {
    FILTER_NEAREST = 0,
    FILTER_LINEAR,
};

enum class AddressMode {
    ADDRESS_MODE_MIRROR,
    ADDRESS_MODE_REPEAT,
    ADDRESS_MODE_CLAMP_TO_EDGE,
    ADDRESS_MODE_CLAMP_TO_BORDER
};

struct SamplerDesc {
    FilterType min_filter;
    FilterType mag_filter;
    MipMapMode mip_map_mode;
    AddressMode address_u;
    AddressMode address_v;
    AddressMode address_w;
    float mMipLodBias;
    bool mSetLodRange;
    float mMinLod;
    float mMaxLod;
    float mMaxAnisotropy;
    CompareFunc mCompareFunc;
};

inline VkFilter util_to_vk_filter(FilterType filter) {
    switch (filter) {
    case FilterType::FILTER_NEAREST:
        return VK_FILTER_NEAREST;
    case FilterType::FILTER_LINEAR:
        return VK_FILTER_LINEAR;
    default:
        return VK_FILTER_LINEAR;
    }
}

inline VkSamplerMipmapMode util_to_vk_mip_map_mode(MipMapMode mipMapMode) {
    switch (mipMapMode) {
    case MipMapMode::MIPMAP_MODE_NEAREST:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case MipMapMode::MIPMAP_MODE_LINEAR:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        LOG_ERROR("invali mipmap mode");
        return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}
inline VkSamplerAddressMode util_to_vk_address_mode(AddressMode addressMode) {
    switch (addressMode) {
    case AddressMode::ADDRESS_MODE_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case AddressMode::ADDRESS_MODE_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
        LOG_ERROR("invali address mode");
        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

inline VkDescriptorType util_to_vk_descriptor_type(DescriptorType type) {
    switch (type) {
    case DESCRIPTOR_TYPE_UNDEFINED:
        assert(false && "Invalid DescriptorInfo Type");
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    case DESCRIPTOR_TYPE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DESCRIPTOR_TYPE_TEXTURE:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DESCRIPTOR_TYPE_RW_TEXTURE:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DESCRIPTOR_TYPE_BUFFER:
    case DESCRIPTOR_TYPE_RW_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    case DESCRIPTOR_TYPE_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
        assert(false && "Invalid DescriptorInfo Type");
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        break;
    }
}


inline Container::Array<char> ReadFile(const char *path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("failed to open shader file: {}", path);
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    Container::Array<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

u32 GetBytesFromTextureFormat(TextureFormat format);

static constexpr u32 MAX_MIP_LEVEL = 12;


struct DrawIndexedInstancedCommand {
    u32 index_count;
    u32 instance_count = 1;
    u32 first_index;
    i32 first_vertex;
    u32 first_instance;
};

enum class RenderTargetLoadOp {
    INVALID,
    DONT_CARE,
    LOAD,
    CLEAR,
};

enum class RenderTargetStoreOp {
    INALID,
    DONT_CARE,
    STORE,
    NONE
};


enum class ShaderOptimizationLevel { DEBUG, O3 };

// only support sm6 now
enum class ShaderTargetProfile {
    // 60
    VS_6_0,
    PS_6_0,
    GS_6_0,
    HS_6_0,
    DS_6_0,
    CS_6_0,
    MS_6_0,
    // 61
    VS_6_1,
    PS_6_1,
    GS_6_1,
    HS_6_1,
    DS_6_1,
    CS_6_1,
    MS_6_1,
    // 62
    VS_6_2,
    PS_6_2,
    GS_6_2,
    HS_6_2,
    DS_6_2,
    CS_6_2,
    MS_6_2,
    // 63
    VS_6_3,
    PS_6_3,
    GS_6_3,
    HS_6_3,
    DS_6_3,
    CS_6_3,
    MS_6_3,
    // 64
    VS_6_4,
    PS_6_4,
    GS_6_4,
    HS_6_4,
    DS_6_4,
    CS_6_4,
    MS_6_4,
    // 65
    VS_6_5,
    PS_6_5,
    GS_6_5,
    HS_6_5,
    DS_6_5,
    CS_6_5,
    MS_6_5,
    // 66
    VS_6_6,
    PS_6_6,
    GS_6_6,
    HS_6_6,
    DS_6_6,
    CS_6_6,
    MS_6_6,
    // 67
    VS_6_7,
    PS_6_7,
    GS_6_7,
    HS_6_7,
    DS_6_7,
    CS_6_7,
    MS_6_7,
};

inline const wchar_t* ToDxcTargetProfile(ShaderTargetProfile tp) {
    switch (tp) {
    case Horizon::ShaderTargetProfile::VS_6_0:
        return L"vs_6_0";
    case Horizon::ShaderTargetProfile::PS_6_0:
        return L"ps_6_0";
    case Horizon::ShaderTargetProfile::GS_6_0:
        return L"gs_6_0";
    case Horizon::ShaderTargetProfile::HS_6_0:
        return L"hs_6_0";
    case Horizon::ShaderTargetProfile::DS_6_0:
        return L"ds_6_0";
    case Horizon::ShaderTargetProfile::CS_6_0:
        return L"cs_6_0";
    case Horizon::ShaderTargetProfile::MS_6_0:
        return L"ms_6_0";
    case Horizon::ShaderTargetProfile::VS_6_1:
        return L"vs_6_1";
    case Horizon::ShaderTargetProfile::PS_6_1:
        return L"ps_6_1";
    case Horizon::ShaderTargetProfile::GS_6_1:
        return L"gs_6_1";
    case Horizon::ShaderTargetProfile::HS_6_1:
        return L"hs_6_1";
    case Horizon::ShaderTargetProfile::DS_6_1:
        return L"ds_6_1";
    case Horizon::ShaderTargetProfile::CS_6_1:
        return L"cs_6_1";
    case Horizon::ShaderTargetProfile::MS_6_1:
        return L"ms_6_1";
    case Horizon::ShaderTargetProfile::VS_6_2:
        return L"vs_6_2";
    case Horizon::ShaderTargetProfile::PS_6_2:
        return L"ps_6_2";
    case Horizon::ShaderTargetProfile::GS_6_2:
        return L"gs_6_2";
    case Horizon::ShaderTargetProfile::HS_6_2:
        return L"hs_6_2";
    case Horizon::ShaderTargetProfile::DS_6_2:
        return L"ds_6_2";
    case Horizon::ShaderTargetProfile::CS_6_2:
        return L"cs_6_2";
    case Horizon::ShaderTargetProfile::MS_6_2:
        return L"ms_6_2";
    case Horizon::ShaderTargetProfile::VS_6_3:
        return L"vs_6_3";
    case Horizon::ShaderTargetProfile::PS_6_3:
        return L"ps_6_3";
    case Horizon::ShaderTargetProfile::GS_6_3:
        return L"gs_6_3";
    case Horizon::ShaderTargetProfile::HS_6_3:
        return L"hs_6_3";
    case Horizon::ShaderTargetProfile::DS_6_3:
        return L"ds_6_3";
    case Horizon::ShaderTargetProfile::CS_6_3:
        return L"cs_6_3";
    case Horizon::ShaderTargetProfile::MS_6_3:
        return L"ms_6_3";
    case Horizon::ShaderTargetProfile::VS_6_4:
        return L"vs_6_4";
    case Horizon::ShaderTargetProfile::PS_6_4:
        return L"ps_6_4";
    case Horizon::ShaderTargetProfile::GS_6_4:
        return L"gs_6_4";
    case Horizon::ShaderTargetProfile::HS_6_4:
        return L"hs_6_4";
    case Horizon::ShaderTargetProfile::DS_6_4:
        return L"ds_6_4";
    case Horizon::ShaderTargetProfile::CS_6_4:
        return L"cs_6_4";
    case Horizon::ShaderTargetProfile::MS_6_4:
        return L"ms_6_4";
    case Horizon::ShaderTargetProfile::VS_6_5:
        return L"vs_6_5";
    case Horizon::ShaderTargetProfile::PS_6_5:
        return L"ps_6_5";
    case Horizon::ShaderTargetProfile::GS_6_5:
        return L"gs_6_5";
    case Horizon::ShaderTargetProfile::HS_6_5:
        return L"hs_6_5";
    case Horizon::ShaderTargetProfile::DS_6_5:
        return L"ds_6_5";
    case Horizon::ShaderTargetProfile::CS_6_5:
        return L"cs_6_5";
    case Horizon::ShaderTargetProfile::MS_6_5:
        return L"ms_6_5";
    case Horizon::ShaderTargetProfile::VS_6_6:
        return L"vs_6_6";
    case Horizon::ShaderTargetProfile::PS_6_6:
        return L"ps_6_6";
    case Horizon::ShaderTargetProfile::GS_6_6:
        return L"gs_6_6";
    case Horizon::ShaderTargetProfile::HS_6_6:
        return L"hs_6_6";
    case Horizon::ShaderTargetProfile::DS_6_6:
        return L"ds_6_6";
    case Horizon::ShaderTargetProfile::CS_6_6:
        return L"cs_6_6";
    case Horizon::ShaderTargetProfile::MS_6_6:
        return L"ms_6_6";
    case Horizon::ShaderTargetProfile::VS_6_7:
        return L"vs_6_7";
    case Horizon::ShaderTargetProfile::PS_6_7:
        return L"ps_6_7";
    case Horizon::ShaderTargetProfile::GS_6_7:
        return L"gs_6_7";
    case Horizon::ShaderTargetProfile::HS_6_7:
        return L"hs_6_7";
    case Horizon::ShaderTargetProfile::DS_6_7:
        return L"ds_6_7";
    case Horizon::ShaderTargetProfile::CS_6_7:
        return L"cs_6_7";
    case Horizon::ShaderTargetProfile::MS_6_7:
        return L"ms_6_7";
    default:
        return L""; // error
    }
}

enum class ShaderTargetAPI {
    SPIRV, DXIL
};

struct ShaderCompilationArgs {
    Container::String entry_point;
    ShaderTargetProfile target_profile;
    ShaderOptimizationLevel optimization_level;
    ShaderTargetAPI target_api;
    Container::String output_file_name;
    Container::String include_path;
};

} // namespace Horizon

