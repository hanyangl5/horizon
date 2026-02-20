#pragma once

#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <core/definations.h>
namespace Horizon
{

// definations

// descriptor set
static constexpr u32 MAX_DESCRIPTOR_SET_INDEX = 4;

// static constexpr u32 MAX_BINDING_PER_DESCRIPTOR_SET = 32;

// render info
static constexpr u32 MAX_RENDER_TARGET_COUNT = 8;

// vertex input
static constexpr u32 MAX_ATTRIBUTE_COUNT = 32;
static constexpr u32 MAX_BINDING_COUNT = 32;

enum class RenderBackend
{
    RENDER_BACKEND_VULKAN,
    RENDER_BACKEND_DX12
};
// always assum queue family index: graphics = 0, compute = 1, transfer = 2
enum CommandQueueType
{
    GRAPHICS = 0,
    COMPUTE,
    TRANSFER
};

enum class PipelineType
{
    UNDIFINED,
    GRAPHICS = 0,
    COMPUTE,
    RAY_TRACING
};

using DescriptorTypes = u32;

enum DescriptorType
{
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

enum class ShaderType
{
    VERTEX_SHADER,
    PIXEL_SHADER,
    GEOMETRY_SHADER,
    DOMAIN_SHADER,
    HULL_SHADER,
    COMPUTE_SHADER,
    MESH_SHADER,
    RAY_GENERATION_SHADER,
    RAY_CLOSEST_HIT_SHADER,
    RAY_MISS_SHADER,
    RAY_ANY_HIT_SHADER,
    MAX_SHADER_TYPE
};

enum ShaderStageFlags
{
    SHADER_STAGE_INVALID = 0,
    SHADER_STAGE_VERTEX_SHADER = 1,
    SHADER_STAGE_PIXEL_SHADER = 2,
    SHADER_STAGE_COMPUTE_SHADER = 4,
    SHADER_STAGE_GEOMETRY_SHADER = 8,
    SHADER_STAGE_DOMAIN_SHADER = 16,
    SHADER_STAGE_HULL_SHADER = 32,
    SHADER_STAGE_MESH_SHADER = 64,
    SHADER_STAGE_RAY_GENERATION_SHADER = 128,
    SHADER_STAGE_RAY_CLOSEST_HIT_SHADER = 256,
    SHADER_STAGE_RAY_MISS_SHADER = 512,
    SHADER_STAGE_RAY_ANY_HIT_SHADER = 1024,
};

enum class TextureType
{
    TEXTURE_TYPE_1D = 0,
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_3D,
    TEXTURE_TYPE_CUBE
};

enum class TextureFormat
{

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

enum ResourceState
{
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

enum class MemoryFlag
{
    DEDICATE_GPU_MEMORY,
    CPU_VISABLE_MEMORY
};

struct BufferCreateInfo
{
    // u32 buffer_usage_flags;
    DescriptorTypes descriptor_types;
    ResourceState initial_state;
    u64 size;
    const char *debug_name;
    // void* data;
};

struct TextureCreateInfo
{
    DescriptorTypes descriptor_types{};
    ResourceState initial_state;
    TextureType texture_type;
    TextureFormat texture_format;
    // TextureUsage texture_usage;
    u32 width, height, depth = 1;
    bool enanble_mipmap = false;
    u32 array_layer = 1;
    const char *debug_name;
};

using SwapChainFormat = TextureFormat;

struct SwapChainCreateInfo
{
    u32 back_buffer_count;
};

// dx12

struct ViewportCreateInfo
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    f32 min_depth;
    f32 max_depth;
};

enum class VertexInputRate
{
    VERTEX_ATTRIB_RATE_VERTEX = 0,
    VERTEX_ATTRIB_RATE_INSTANCE = 1,
};

enum class VertexAttribFormat
{
    U8,
    U16,
    U32,
    S8,
    S16,
    S32,
    F16,
    F32,
    UN8,
    UN16,
    SN8,
    SN16
};

struct VertexAttributeDescription
{
    VertexAttribFormat attrib_format;
    u32 portion;
    VertexInputRate input_rate;
    u32 location;
    u32 binding; // use to seperate use seperated buffer (e.g. position, normal) of vertex stream
    u32 stride;
    u32 offset;
    // DX12 semantic information (optional, can be empty for Vulkan)
    const char *semantic_name = nullptr;
    u32 semantic_index = 0;
};

struct VertexInputState
{
    u32 attribute_count;
    VertexAttributeDescription attributes[MAX_ATTRIBUTE_COUNT];
};

enum class PrimitiveTopology
{
    POINT_LIST,
    LINE_LIST,
    TRIANGLE_LIST
};

struct InputAssemblyState
{
    PrimitiveTopology topology;
};

struct ViewPortState
{
    u32 width;
    u32 height;
};

enum class FrontFace
{
    CCW,
    CW
};

enum class FillMode
{
    POINT,
    LINE,
    TRIANGLE
};

enum class CullMode
{
    NONE,
    FRONT,
    BACK,
    ALL
};

struct RasterizationState
{
    FrontFace front_face;
    CullMode cull_mode;
    FillMode fill_mode;
    bool discard;
};

enum class CompareFunc
{
    NEVER,
    LESS,
    L_EQUAL,
    EQUAL,
    GREATER,
    G_EQUAL,
    ALWAYS
};

using DepthFunc = CompareFunc;

struct DepthStencilState
{
    bool depth_test;
    bool depth_write;
    bool stencil_enabled;
    DepthFunc depth_func;
    TextureFormat depth_stencil_format;
    f32 depthNear, depthFar;
    // stencil settings
};

struct MultiSampleState
{
    u32 sample_count;
};

struct RenderTargetFormats
{
    u32 color_attachment_count = 0;
    std::vector<TextureFormat> color_attachment_formats;
    bool has_depth = true, has_stencil = false;
    TextureFormat depth_stencil_format;
};

struct ShaderPrograms
{
  public:
    void *VertexShader() const
    {
        return m_shaders[(u32)ShaderType::VERTEX_SHADER];
    }
    void *PixelShader() const
    {
        return m_shaders[(u32)ShaderType::PIXEL_SHADER];
    }
    void *ComputeShader() const
    {
        return m_shaders[(u32)ShaderType::COMPUTE_SHADER];
    }
    void *GeometryShader() const
    {
        return m_shaders[(u32)ShaderType::GEOMETRY_SHADER];
    }
    void *DomainShader() const
    {
        return m_shaders[(u32)ShaderType::DOMAIN_SHADER];
    }
    void *HullShader() const
    {
        return m_shaders[(u32)ShaderType::HULL_SHADER];
    }
    void *MeshShader() const
    {
        return m_shaders[(u32)ShaderType::MESH_SHADER];
    }
    void *RayGenerationShader() const
    {
        return m_shaders[(u32)ShaderType::RAY_GENERATION_SHADER];
    }
    void *RayClosestHitShader() const
    {
        return m_shaders[(u32)ShaderType::RAY_CLOSEST_HIT_SHADER];
    }
    void *RayMissShader() const
    {
        return m_shaders[(u32)ShaderType::RAY_MISS_SHADER];
    }
    void *RayAnyHitShader() const
    {
        return m_shaders[(u32)ShaderType::RAY_ANY_HIT_SHADER];
    }
    void SetShader(ShaderType type, void *shader)
    {
        m_shaders[(u32)type] = shader;
    };

  private:
    void *m_shaders[(u32)ShaderType::MAX_SHADER_TYPE];
};

struct GraphicsPipelineCreateInfo
{
    ShaderPrograms shader_program;
    VertexInputState vertex_input_state;
    InputAssemblyState input_assembly_state;
    ViewPortState view_port_state;
    RasterizationState rasterization_state;
    DepthStencilState depth_stencil_state;
    MultiSampleState multi_sample_state;
    RenderTargetFormats render_target_formats;
};

struct ComputePipelineCreateInfo
{
    ShaderPrograms shader_program;
    u32 flag = 0x01;
};

// struct PipelineCreateInfo
//{
//    PipelineType type;
//    GraphicsPipelineCreateInfo *gpci;
//    ComputePipelineCreateInfo *cpci;
//};

struct Rect
{
    u32 x, y, w, h;
};

using RenderTargetFormat = TextureFormat;

enum class RenderTargetType
{
    COLOR,
    DEPTH_STENCIL,
    UNDEFINED
};

struct RenderTargetCreateInfo
{
    RenderTargetFormat rt_format;
    RenderTargetType rt_type;
    u32 width, height;
};

struct DrawParam
{
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    i32 vertexOffset;
    u32 firstInstance;
};

// enum class ResourceUpdateFrequency { NONE, PER_FRAME, PER_BATCH, PER_DRAW, BINDLESS, USER_DEFINED1 };
constexpr u32 BINDLESS_DESCRIPTOR_SET_NUMBER = 1;
constexpr u32 DEFAULT_DESCRIPTOR_SET_NUMBER = 0;
struct DescriptorDesc
{
    DescriptorType type{};
    u32 vk_binding{};
    // std::string dx_reg{}; // todo : type -> reg type
};

struct PushConstantDesc
{
    u32 size;
    u32 offset;
    u32 shader_stages;
};

struct RootSignatureDesc
{
    std::map<u32, std::unordered_map<std::string, DescriptorDesc>> descriptors{};
    std::unordered_map<std::string, PushConstantDesc> push_constants;
};

u32 GetStrideFromVertexAttributeDescription(VertexAttribFormat format, u32 portions);

struct VkPipelineLayoutDesc
{
  public:
    u64 descriptor_set_hash_key;
    u64 bindless_descriptor_set_hash_key;
    // std::array<u64, MAX_DESCRIPTOR_SET_INDEX> descriptor_set_hash_key{};
};

ShaderStageFlags GetShaderStageFlagsFromShaderType(ShaderType type);

typedef union ClearColorValue {
    f32 float32[4];
    i32 int32[4];
    u32 uint32[4];
} ClearColorValue;

struct ClearValueDepthStencil
{
    f32 depth;
    u32 stencil;
};

struct TextureDataDesc
{
    u32 width;
    u32 height;
    u32 depth;
    u32 layer_count = 1;
    u32 mipmap_count = 1;
    TextureFormat format;
    TextureType type;
    std::vector<char> raw_data;
    // we don't have a unified runtime format for mipmap/layer, so we have to store that
    std::vector<std::vector<u32>> data_offset_map;
};

struct BufferUpdateDesc
{
    void *data;
    u64 size;
    u64 offset;
};

struct TextureUpdateDesc
{
    u64 size = 0;
    // u32 row_length;
    // u32 height;
    u32 first_mip_level = 0;
    u32 mip_level_count = 1;
    u32 first_layer = 0;
    u32 layer_count = 1;
    TextureDataDesc *texture_data_desc{};
};

enum class MipMapMode
{
    MIPMAP_MODE_NEAREST = 0,
    MIPMAP_MODE_LINEAR
};

enum class FilterType
{
    FILTER_NEAREST = 0,
    FILTER_LINEAR,
};

enum class AddressMode
{
    ADDRESS_MODE_MIRROR,
    ADDRESS_MODE_REPEAT,
    ADDRESS_MODE_CLAMP_TO_EDGE,
    ADDRESS_MODE_CLAMP_TO_BORDER
};

struct SamplerDesc
{
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

u32 GetBytesFromTextureFormat(TextureFormat format);

static constexpr u32 MAX_MIP_LEVEL = 12;

struct DrawIndexedInstancedCommand
{
    u32 index_count;
    u32 instance_count = 1;
    u32 first_index;
    i32 vertex_offset;
    u32 first_instance;
};

enum class RenderTargetLoadOp
{
    INVALID,
    DONT_CARE,
    LOAD,
    CLEAR,
};

enum class RenderTargetStoreOp
{
    INALID,
    DONT_CARE,
    STORE,
    NONE
};

enum class EMeshAssetFormat
{
    MESH_FORMAT_NONE,
    MESH_FORMAT_GLTF,
    MESH_FORMAT_GLB,
    // MESH_TYPE_COLLADA,
    // MESH_TYPE_FBX,
    // MESH_TYPE_OBJ,
    // MESH_TYPE_STL,
    // MESH_TYPE_PLY,
    // MESH_TYPE_DAE,
    // MESH_TYPE_DAE,
};

} // namespace Horizon
