#pragma once

#include <array>

#include "dx12/stdafx.h"
#include <d3d12.h>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/vulkan/VulkanConfig.h>

namespace Horizon {

enum class RenderBackend {
    RENDER_BACKEND_NONE,
    RENDER_BACKEND_VULKAN,
    RENDER_BACKEND_DX12
};
// always assum queue family index: graphics = 0, compute = 1, transfer = 2
enum CommandQueueType { GRAPHICS = 0, COMPUTE, TRANSFER };

enum class PipelineType { GRAPHICS = 0, COMPUTE, RAY_TRACING };

enum DescriptorType {
    DESCRIPTOR_TYPE_UNDEFINED = 0,
    DESCRIPTOR_TYPE_SAMPLER = 0x01,
    // SRV Read only texture
    DESCRIPTOR_TYPE_TEXTURE = (DESCRIPTOR_TYPE_SAMPLER << 1),
    /// UAV Texture
    DESCRIPTOR_TYPE_RW_TEXTURE = (DESCRIPTOR_TYPE_TEXTURE << 1),
    // SRV Read only buffer
    DESCRIPTOR_TYPE_BUFFER = (DESCRIPTOR_TYPE_RW_TEXTURE << 1),
    DESCRIPTOR_TYPE_BUFFER_RAW =
        (DESCRIPTOR_TYPE_BUFFER | (DESCRIPTOR_TYPE_BUFFER << 1)),
    /// UAV Buffer
    DESCRIPTOR_TYPE_RW_BUFFER = (DESCRIPTOR_TYPE_BUFFER << 2),
    DESCRIPTOR_TYPE_RW_BUFFER_RAW =
        (DESCRIPTOR_TYPE_RW_BUFFER | (DESCRIPTOR_TYPE_RW_BUFFER << 1)),
    /// Uniform buffer
    DESCRIPTOR_TYPE_CONSTANT_BUFFER = (DESCRIPTOR_TYPE_RW_BUFFER << 2),
    /// Push constant / Root constant
    DESCRIPTOR_TYPE_ROOT_CONSTANT = (DESCRIPTOR_TYPE_CONSTANT_BUFFER << 1),
    /// IA
    DESCRIPTOR_TYPE_VERTEX_BUFFER = (DESCRIPTOR_TYPE_ROOT_CONSTANT << 1),
    DESCRIPTOR_TYPE_INDEX_BUFFER = (DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
    DESCRIPTOR_TYPE_INDIRECT_BUFFER = (DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
    /// Cubemap SRV
    DESCRIPTOR_TYPE_TEXTURE_CUBE =
        (DESCRIPTOR_TYPE_TEXTURE | (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1)),
    /// RTV / DSV per mip slice
    DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES =
        (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 2),
    /// RTV / DSV per array slice
    DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES =
        (DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES << 1),
    /// RTV / DSV per depth slice
    DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES =
        (DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
    DESCRIPTOR_TYPE_RAY_TRACING =
        (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
#if defined(USE_VULKAN)
    /// Subpass input (descriptor type only available in Vulkan)
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (DESCRIPTOR_TYPE_RAY_TRACING << 1),
    DESCRIPTOR_TYPE_TEXEL_BUFFER = (DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
    DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER =
        (DESCRIPTOR_TYPE_RW_TEXEL_BUFFER << 1),

    /// Khronos extension ray tracing
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE =
        (DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER << 1),
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT =
        (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE << 1),
    DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS =
        (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT << 1),
    DESCRIPTOR_TYPE_SHADER_BINDING_TABLE =
        (DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS << 1),
#endif

};

// using DescriptorType = u32;

enum class ShaderType {
    VERTEX_SHADER,
    PIXEL_SHADER,
    COMPUTE_SHADER,
    // GEOMETRY_SHADER,
    //  ray tracing related shader
};

enum ShaderStageFlags {
    SHADER_STAGE_VERTEX_SHADER = 1,
    SHADER_STAGE_PIXEL_SHADER = 2,
    SHADER_STAGE_COMPUTE_SHADER = 4,
};

enum class TextureType {
    TEXTURE_TYPE_1D = 0,
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_3D,
};

enum class TextureFormat {
    // unsigned int
    TEXTURE_FORMAT_R8_UINT = 0,
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

    // normalized unsinged int
    TEXTURE_FORMAT_R8_UNORM,
    TEXTURE_FORMAT_RG8_UNORM,
    TEXTURE_FORMAT_RGB8_UNORM,
    TEXTURE_FORMAT_RGBA8_UNORM,

    TEXTURE_FORMAT_R16_UNORM,
    TEXTURE_FORMAT_RG16_UNORM,
    TEXTURE_FORMAT_RGB16_UNORM,
    TEXTURE_FORMAT_RGBA16_UNORM,

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

    // normalized signed int
    TEXTURE_FORMAT_R8_SNORM,
    TEXTURE_FORMAT_RG8_SNORM,
    TEXTURE_FORMAT_RGB8_SNORM,
    TEXTURE_FORMAT_RGBA8_SNORM,

    TEXTURE_FORMAT_R16_SNORM,
    TEXTURE_FORMAT_RG16_SNORM,
    TEXTURE_FORMAT_RGB16_SNORM,
    TEXTURE_FORMAT_RGBA16_SNORM,

    TEXTURE_FORMAT_R32_SNORM,
    TEXTURE_FORMAT_RG32_SNORM,
    TEXTURE_FORMAT_RGB32_SNORM,
    TEXTURE_FORMAT_RGBA32_SNORM,

    // signed float

    TEXTURE_FORMAT_R16_SFLOAT,
    TEXTURE_FORMAT_RG16_SFLOAT,
    TEXTURE_FORMAT_RGB16_SFLOAT,
    TEXTURE_FORMAT_RGBA16_SFLOAT,

    TEXTURE_FORMAT_R32_SFLOAT,
    TEXTURE_FORMAT_RG32_SFLOAT,
    TEXTURE_FORMAT_RGB32_SFLOAT,
    TEXTURE_FORMAT_RGBA32_SFLOAT,

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
    RESOURCE_STATE_GENERIC_READ =
        (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
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
    DescriptorType descriptor_type;
    ResourceState initial_state;
    u64 size;
    // void* data;
};

struct TextureCreateInfo {
    DescriptorType descriptor_type;
    ResourceState initial_state;
    TextureType texture_type;
    TextureFormat texture_format;
    // TextureUsage texture_usage;
    u32 width, height, depth = 1;
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
//     std::vector<VkVertexInputBindingDescription> binding_descriptions;
//     std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
// };
//
// struct VertexInputStateDx12 {
//     std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_descs;
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

enum class VertexAttribFormat { U8, U16, U32, F8, F16, F32, NF8, NF16, NF32 };

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
    static constexpr u32 MAX_ATTRIBUTE_COUNT = 32;
    static constexpr u32 MAX_BINDING_COUNT = 32;
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

enum class DepthFunc {
    NEVER,
    LESS,
    L_EQUAL,
    EQUAL,
    GREATER,
    G_EQUAL,
    ALWAYS

};

struct DepthStencilState {
    bool depth_test;
    bool depth_write;
    DepthFunc depth_func;
    TextureFormat depth_stencil_format;
    f32 depthRange[2]; // [0.0, 1.0]
    // stencil settings
};

struct MultiSampleState {
    u32 sample_count;
};

struct GraphicsPipelineCreateInfo {
    VertexInputState vertex_input_state;
    InputAssemblyState input_assembly_state;
    ViewPortState view_port_state;
    RasterizationState raserization_state;
    DepthStencilState depth_stencil_state;
    MultiSampleState multi_sample_state;
};

struct ComputePipelineCreateInfo {
    u32 flag = 0x01;
};

struct PipelineCreateInfo {
    PipelineType type;
    GraphicsPipelineCreateInfo *gpci;
    ComputePipelineCreateInfo *cpci;
};

struct PipelineState {};

struct Rect {
    u32 x, y, w, h;
};

using RenderTargetFormat = TextureFormat;

enum class RenderTargetType { COLOR, DEPTH_STENCIL };

struct RenderTargetCreateInfo {
    RenderTargetFormat rt_format;
    RenderTargetType rt_type;
    u32 width, height;
};

class RenderTarget;
struct RenderPassBeginInfo {
    static constexpr u32 MAX_RENDER_TARGET_COUNT = 8;
    std::array<RenderTarget *, MAX_RENDER_TARGET_COUNT> render_targets;
    RenderTarget *depth, *stencil;
    Rect render_area;
};

struct DrawParam {
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    i32 vertexOffset;
    u32 firstInstance;
};

u32 GetStrideFromVertexAttributeDescription(VertexAttribFormat format,
                                            u32 portions);
} // namespace Horizon