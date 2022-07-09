#pragma once

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

inline D3D12_COMMAND_LIST_TYPE ToDX12CommandQueueType(CommandQueueType type) {
    switch (type) {
    case Horizon::GRAPHICS:
        return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case Horizon::COMPUTE:
        return D3D12_COMMAND_LIST_TYPE_COMPUTE;
    case Horizon::TRANSFER:
        return D3D12_COMMAND_LIST_TYPE_COPY;
    default:
        LOG_ERROR("invalid command queue type")
        return {};
    }
}

enum class PipelineType { GRAPHICS = 0, COMPUTE, RAY_TRACING };

struct PipelineCreateInfo {
    PipelineType type;
};

inline VkPipelineBindPoint ToVkPipelineBindPoint(PipelineType type) {
    switch (type) {
    case Horizon::PipelineType::GRAPHICS:
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    case Horizon::PipelineType::COMPUTE:
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    case Horizon::PipelineType::RAY_TRACING:
        return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
    default:
        LOG_ERROR("invalid pipeline type");
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    }
}

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
    DESCRIPTOR_TYPE_UNIFORM_BUFFER = (DESCRIPTOR_TYPE_RW_BUFFER << 2),
    /// Push constant / Root constant
    DESCRIPTOR_TYPE_ROOT_CONSTANT = (DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1),
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
    GEOMETRY_SHADER,
    // ray tracing related shader
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

inline VkAccessFlags util_to_vk_access_flags(ResourceState state) {
    VkAccessFlags ret = 0;

    if (state & RESOURCE_STATE_HOST_READ) {
        ret |= VK_ACCESS_HOST_READ_BIT;
    }
    if (state & RESOURCE_STATE_HOST_WRITE) {
        ret |= VK_ACCESS_HOST_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_COPY_SOURCE) {
        ret |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (state & RESOURCE_STATE_COPY_DEST) {
        ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) {
        ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (state & RESOURCE_STATE_INDEX_BUFFER) {
        ret |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (state & RESOURCE_STATE_UNORDERED_ACCESS) {
        ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_INDIRECT_ARGUMENT) {
        ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (state & RESOURCE_STATE_RENDER_TARGET) {
        ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_DEPTH_WRITE) {
        ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_SHADER_RESOURCE) {
        ret |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (state & RESOURCE_STATE_PRESENT) {
        ret |= VK_ACCESS_MEMORY_READ_BIT;
    }
    //#if defined(QUEST_VR)
    //    if (state & RESOURCE_STATE_SHADING_RATE_SOURCE) {
    //        ret |= VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
    //    }
    //#endif
    //
    //#ifdef VK_RAYTRACING_AVAILABLE
    //    if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) {
    //        ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
    //               VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    //    }
    //#endif

    return ret;
}

inline VkImageLayout util_to_vk_image_layout(ResourceState usage) {
    if (usage & RESOURCE_STATE_COPY_SOURCE)
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    if (usage & RESOURCE_STATE_COPY_DEST)
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    if (usage & RESOURCE_STATE_RENDER_TARGET)
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    if (usage & RESOURCE_STATE_DEPTH_WRITE)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (usage & RESOURCE_STATE_UNORDERED_ACCESS)
        return VK_IMAGE_LAYOUT_GENERAL;

    if (usage & RESOURCE_STATE_SHADER_RESOURCE)
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (usage & RESOURCE_STATE_PRESENT)
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    if (usage == RESOURCE_STATE_COMMON)
        return VK_IMAGE_LAYOUT_GENERAL;

#if defined(QUEST_VR)
    if (usage == RESOURCE_STATE_SHADING_RATE_SOURCE)
        return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
#endif

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

inline VkImageUsageFlags util_to_vk_image_usage(DescriptorType usage) {
    VkImageUsageFlags result = 0;
    if (DESCRIPTOR_TYPE_TEXTURE == (usage & DESCRIPTOR_TYPE_TEXTURE))
        result |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (DESCRIPTOR_TYPE_RW_TEXTURE == (usage & DESCRIPTOR_TYPE_RW_TEXTURE))
        result |= VK_IMAGE_USAGE_STORAGE_BIT;
    return result;
}

inline VkPipelineStageFlags
util_determine_pipeline_stage_flags(VkAccessFlags accessFlags,
                                    CommandQueueType queueType) {
    VkPipelineStageFlags flags = 0;

    switch (queueType) {
    case GRAPHICS: {
        if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT |
                            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

        if ((accessFlags &
             (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
              VK_ACCESS_SHADER_WRITE_BIT)) != 0) {
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            // if (pRenderer->pActiveGpuSettings->mGeometryShaderSupported) {
            //     flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
            // }
            // if (pRenderer->pActiveGpuSettings->mTessellationSupported) {
            //     flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
            //     flags |=
            //     VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
            // }
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            //#ifdef VK_RAYTRACING_AVAILABLE
            //            if (pRenderer->mVulkan.mRaytracingSupported) {
            //                flags |=
            //                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            //            }
            //#endif
        }

        if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

#if defined(QUEST_VR)
        if ((accessFlags & VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT) != 0)
            flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
#endif
        break;
    }
    case COMPUTE: {
        if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT |
                            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
            (accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
            (accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
            (accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if ((accessFlags &
             (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
              VK_ACCESS_SHADER_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        break;
    }
    case TRANSFER:
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    default:
        break;
    }

    // Compatible with both compute and graphics queues
    if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
        flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

    if ((accessFlags &
         (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

    if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) !=
        0)
        flags |= VK_PIPELINE_STAGE_HOST_BIT;

    if (flags == 0)
        flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    return flags;
}

inline VkShaderStageFlags ToVkShaderStageFlags(u32 stage) noexcept {
    VkShaderStageFlags flags = 0;
    if (stage & SHADER_STAGE_VERTEX_SHADER) {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (stage & SHADER_STAGE_PIXEL_SHADER) {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (stage & SHADER_STAGE_COMPUTE_SHADER) {
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    return flags;
}

inline VkImageType ToVkImageType(TextureType type) noexcept {
    switch (type) {
    case Horizon::TextureType::TEXTURE_TYPE_1D:
        return VK_IMAGE_TYPE_1D;
    case Horizon::TextureType::TEXTURE_TYPE_2D:
        return VK_IMAGE_TYPE_2D;
    case Horizon::TextureType::TEXTURE_TYPE_3D:
        return VK_IMAGE_TYPE_3D;
    default:
        LOG_ERROR("invalid image type");
        return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

inline VkFormat ToVkImageFormat(TextureFormat format) noexcept {
    switch (format) {
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UINT:
        return VK_FORMAT_R8_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UINT:
        return VK_FORMAT_R8G8_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UINT:
        return VK_FORMAT_R8G8B8_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UINT:
        return VK_FORMAT_R8G8B8A8_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UINT:
        return VK_FORMAT_R16_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UINT:
        return VK_FORMAT_R16G16B16_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_UINT:
        return VK_FORMAT_R32_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UNORM:
        return VK_FORMAT_R8_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UNORM:
        return VK_FORMAT_R8G8B8_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UNORM:
        return VK_FORMAT_R16G16B16_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SINT:
        return VK_FORMAT_R8_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SINT:
        return VK_FORMAT_R8G8_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SINT:
        return VK_FORMAT_R8G8B8_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SINT:
        return VK_FORMAT_R8G8B8A8_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SINT:
        return VK_FORMAT_R16_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SINT:
        return VK_FORMAT_R16G16B16_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SINT:
        return VK_FORMAT_R32_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SINT:
        return VK_FORMAT_R32G32B32_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Horizon::TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    default:
        LOG_ERROR("invalid format");
        return VK_FORMAT_MAX_ENUM;
    }
}

inline VkImageAspectFlags ToVkAspectMaskFlags(VkFormat format,
                                              bool includeStencilBit) {
    VkImageAspectFlags result = 0;
    switch (format) {
        // Depth
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        result = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
        // Stencil
    case VK_FORMAT_S8_UINT:
        result = VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
        // Depth/stencil
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        result = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (includeStencilBit)
            result |= VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
        // Assume everything else is Color
    default:
        result = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
    }
    return result;
}

inline VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorType usage,
                                                  bool typed) {
    VkBufferUsageFlags result = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (usage & DESCRIPTOR_TYPE_RW_BUFFER) {
        result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (typed)
            result |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }
    if (usage & DESCRIPTOR_TYPE_BUFFER) {
        result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (typed)
            result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }
    if (usage & DESCRIPTOR_TYPE_INDEX_BUFFER) {
        result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (usage & DESCRIPTOR_TYPE_VERTEX_BUFFER) {
        result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (usage & DESCRIPTOR_TYPE_INDIRECT_BUFFER) {
        result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
#ifdef VK_RAYTRACING_AVAILABLE
    if (usage & DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE) {
        result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }
    if (usage & DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT) {
        result |=
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    if (usage & DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS) {
        result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    if (usage & DESCRIPTOR_TYPE_SHADER_BINDING_TABLE) {
        result |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    }
#endif
    return result;
}

// dx12

inline DXGI_FORMAT ToDx12TextureFormat(TextureFormat format) {
    switch (format) {
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_UINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SINT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SNORM:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT:
        break;
    case Horizon::TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
        break;
    default:
        LOG_ERROR("invalid texture format, use rgba8 format as default");
        return DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

inline D3D12_RESOURCE_DIMENSION ToDX12TextureDimension(TextureType type) {
    switch (type) {
    case Horizon::TextureType::TEXTURE_TYPE_1D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        break;
    case Horizon::TextureType::TEXTURE_TYPE_2D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        break;
    case Horizon::TextureType::TEXTURE_TYPE_3D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        break;
    default:
        LOG_ERROR("invalid image type, use texture2D as default");
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        break;
    }
}

} // namespace Horizon