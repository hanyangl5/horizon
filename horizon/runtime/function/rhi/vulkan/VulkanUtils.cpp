#pragma once

#include "VulkanUtils.h"

namespace Horizon {

VkPipelineBindPoint ToVkPipelineBindPoint(PipelineType type) noexcept {
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

VkShaderStageFlagBits ToVkShaderStageBit(ShaderType type) noexcept {
    switch (type) {
    case Horizon::ShaderType::VERTEX_SHADER:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case Horizon::ShaderType::PIXEL_SHADER:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case Horizon::ShaderType::COMPUTE_SHADER:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        break;
    }
}

VkAccessFlags util_to_vk_access_flags(ResourceState state) noexcept {
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
        ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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

VkImageLayout util_to_vk_image_layout(ResourceState usage) noexcept {
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

VkImageUsageFlags util_to_vk_image_usage(DescriptorTypes types) noexcept {
    VkImageUsageFlags result = 0;
    if (DESCRIPTOR_TYPE_TEXTURE == (types & DESCRIPTOR_TYPE_TEXTURE))
        result |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (DESCRIPTOR_TYPE_RW_TEXTURE == (types & DESCRIPTOR_TYPE_RW_TEXTURE))
        result |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (DESCRIPTOR_TYPE_COLOR_ATTACHMENT == (types & DESCRIPTOR_TYPE_COLOR_ATTACHMENT))
        result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT == (types & DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT))
        result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    assert(result != 0);
    return result;
}

VkPipelineStageFlags util_determine_pipeline_stage_flags(VkAccessFlags accessFlags,
                                                         CommandQueueType queueType) noexcept {
    VkPipelineStageFlags flags = 0;

    switch (queueType) {
    case GRAPHICS: {
        if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

        if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) !=
            0) {
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

        if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        if ((accessFlags &
             (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

#if defined(QUEST_VR)
        if ((accessFlags & VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT) != 0)
            flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
#endif
        break;
    }
    case COMPUTE: {
        if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
            (accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
            (accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
            (accessFlags &
             (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
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

    if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

    if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
        flags |= VK_PIPELINE_STAGE_HOST_BIT;

    if (flags == 0)
        flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    return flags;
}

VkShaderStageFlags ToVkShaderStageFlags(u32 stage) noexcept {
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

VkImageType ToVkImageType(TextureType type) noexcept {
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

VkFormat ToVkImageFormat(TextureFormat format) noexcept {
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

    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SNORM:
        return VK_FORMAT_R8_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SNORM:
        return VK_FORMAT_R8G8_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SNORM:
        return VK_FORMAT_R8G8B8_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SNORM:
        return VK_FORMAT_R16_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SNORM:
        return VK_FORMAT_R16G16B16_SNORM;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;

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
    //case Horizon::TextureFormat::TEXTURE_FORMAT_R11G11B10_SFLOAT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    default:
        LOG_ERROR("invalid format");
        return VK_FORMAT_MAX_ENUM;
    }
}

VkImageAspectFlags ToVkAspectMaskFlags(VkFormat format, bool includeStencilBit) noexcept {
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

VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorTypes usage, bool typed) noexcept {
    VkBufferUsageFlags result = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & DESCRIPTOR_TYPE_CONSTANT_BUFFER) {
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
        result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
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

VkFormat ToVkImageFormat(VertexAttribFormat format, u32 portions) noexcept {

    if (portions == 1) {
        switch (format) {
        case Horizon::VertexAttribFormat::U8:
            return VK_FORMAT_R8_UINT;
        case Horizon::VertexAttribFormat::U16:
            return VK_FORMAT_R16_UINT;
        case Horizon::VertexAttribFormat::U32:
            return VK_FORMAT_R32_UINT;
        case Horizon::VertexAttribFormat::S8:
            return VK_FORMAT_R8_SINT;
        case Horizon::VertexAttribFormat::S16:
            return VK_FORMAT_R16_SINT;
        case Horizon::VertexAttribFormat::S32:
            return VK_FORMAT_R32_SINT;
        case Horizon::VertexAttribFormat::F16:
            return VK_FORMAT_R16_SFLOAT;
        case Horizon::VertexAttribFormat::F32:
            return VK_FORMAT_R32_SFLOAT;
        case Horizon::VertexAttribFormat::UN8:
            return VK_FORMAT_R8_UNORM;
        case Horizon::VertexAttribFormat::UN16:
            return VK_FORMAT_R16_UNORM;
        case Horizon::VertexAttribFormat::SN8:
            return VK_FORMAT_R8_SNORM;
        case Horizon::VertexAttribFormat::SN16:
            return VK_FORMAT_R16_SNORM;
        default:
            return VK_FORMAT_MAX_ENUM;
        }
    } else if (portions == 2) {
        switch (format) {
        case Horizon::VertexAttribFormat::U8:
            return VK_FORMAT_R8G8_UINT;
        case Horizon::VertexAttribFormat::U16:
            return VK_FORMAT_R16G16_UINT;
        case Horizon::VertexAttribFormat::U32:
            return VK_FORMAT_R32G32_UINT;
        case Horizon::VertexAttribFormat::S8:
            return VK_FORMAT_R8G8_SINT;
        case Horizon::VertexAttribFormat::S16:
            return VK_FORMAT_R16G16_SINT;
        case Horizon::VertexAttribFormat::S32:
            return VK_FORMAT_R32G32_SINT;
        case Horizon::VertexAttribFormat::F16:
            return VK_FORMAT_R16G16_SFLOAT;
        case Horizon::VertexAttribFormat::F32:
            return VK_FORMAT_R32G32_SFLOAT;
        case Horizon::VertexAttribFormat::UN8:
            return VK_FORMAT_R8G8_UNORM;
        case Horizon::VertexAttribFormat::UN16:
            return VK_FORMAT_R16G16_UNORM;
        case Horizon::VertexAttribFormat::SN8:
            return VK_FORMAT_R8G8_SNORM;
        case Horizon::VertexAttribFormat::SN16:
            return VK_FORMAT_R16G16_SNORM;
        default:
            return VK_FORMAT_MAX_ENUM;
        }
    } else if (portions == 3) {
        switch (format) {
        case Horizon::VertexAttribFormat::U8:
            return VK_FORMAT_R8G8B8_UINT;
        case Horizon::VertexAttribFormat::U16:
            return VK_FORMAT_R16G16B16_UINT;
        case Horizon::VertexAttribFormat::U32:
            return VK_FORMAT_R32G32B32_UINT;
        case Horizon::VertexAttribFormat::S8:
            return VK_FORMAT_R8G8B8_SINT;
        case Horizon::VertexAttribFormat::S16:
            return VK_FORMAT_R16G16B16_SINT;
        case Horizon::VertexAttribFormat::S32:
            return VK_FORMAT_R32G32B32_SINT;
        case Horizon::VertexAttribFormat::F16:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case Horizon::VertexAttribFormat::F32:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case Horizon::VertexAttribFormat::UN8:
            return VK_FORMAT_R8G8B8_UNORM;
        case Horizon::VertexAttribFormat::UN16:
            return VK_FORMAT_R16G16B16_UNORM;
        case Horizon::VertexAttribFormat::SN8:
            return VK_FORMAT_R8G8B8_SNORM;
        case Horizon::VertexAttribFormat::SN16:
            return VK_FORMAT_R16G16B16_SNORM;
        default:
            return VK_FORMAT_MAX_ENUM;
        }
    } else if (portions == 4) {
        switch (format) {
        case Horizon::VertexAttribFormat::U8:
            return VK_FORMAT_R8G8B8A8_UINT;
        case Horizon::VertexAttribFormat::U16:
            return VK_FORMAT_R16G16B16A16_UINT;
        case Horizon::VertexAttribFormat::U32:
            return VK_FORMAT_R32G32B32A32_UINT;
        case Horizon::VertexAttribFormat::S8:
            return VK_FORMAT_R8G8B8A8_SINT;
        case Horizon::VertexAttribFormat::S16:
            return VK_FORMAT_R16G16B16A16_SINT;
        case Horizon::VertexAttribFormat::S32:
            return VK_FORMAT_R32G32B32A32_SINT;
        case Horizon::VertexAttribFormat::F16:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Horizon::VertexAttribFormat::F32:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Horizon::VertexAttribFormat::UN8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Horizon::VertexAttribFormat::UN16:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case Horizon::VertexAttribFormat::SN8:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case Horizon::VertexAttribFormat::SN16:
            return VK_FORMAT_R16G16B16A16_SNORM;
        default:
            return VK_FORMAT_MAX_ENUM;
        }
    } else {
        return VK_FORMAT_MAX_ENUM;
    }
}

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology t) noexcept {
    switch (t) {
    case Horizon::PrimitiveTopology::POINT_LIST:
        return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;
    case Horizon::PrimitiveTopology::LINE_LIST:
        return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    case Horizon::PrimitiveTopology::TRIANGLE_LIST:
        return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    default:
        return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        break;
    }
}

VkFrontFace ToVkFrontFace(FrontFace front_face) noexcept {
    switch (front_face) {
    case Horizon::FrontFace::CCW:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        break;
    case Horizon::FrontFace::CW:
        return VK_FRONT_FACE_CLOCKWISE;
        break;
    default:
        return VK_FRONT_FACE_MAX_ENUM;
        break;
    }
}

VkCullModeFlagBits ToVkCullMode(CullMode cull_mode) noexcept {
    switch (cull_mode) {
    case Horizon::CullMode::NONE:
        return VkCullModeFlagBits::VK_CULL_MODE_NONE;
        break;
    case Horizon::CullMode::FRONT:
        return VkCullModeFlagBits::VK_CULL_MODE_FRONT_BIT;
        break;
    case Horizon::CullMode::BACK:
        return VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT;
        break;
    case Horizon::CullMode::ALL:
        return VkCullModeFlagBits::VK_CULL_MODE_FRONT_AND_BACK;
        break;
    default:
        return VkCullModeFlagBits::VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
        break;
    }
}

VkPolygonMode ToVkPolygonMode(FillMode fill_mode) noexcept {

    switch (fill_mode) {
    case Horizon::FillMode::POINT:
        return VK_POLYGON_MODE_POINT;
        break;
    case Horizon::FillMode::LINE:
        return VK_POLYGON_MODE_LINE;
        break;
    case Horizon::FillMode::TRIANGLE:
        return VK_POLYGON_MODE_FILL;
        break;
    default:
        return VK_POLYGON_MODE_MAX_ENUM;
        break;
    }
}

VkCompareOp ToVkCompareOp(DepthFunc depth_func) noexcept {
    switch (depth_func) {
    case Horizon::DepthFunc::NEVER:
        return VkCompareOp::VK_COMPARE_OP_NEVER;
        break;
    case Horizon::DepthFunc::LESS:
        return VkCompareOp::VK_COMPARE_OP_LESS;
        break;
    case Horizon::DepthFunc::L_EQUAL:
        return VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
    case Horizon::DepthFunc::EQUAL:
        return VkCompareOp::VK_COMPARE_OP_EQUAL;
        break;
    case Horizon::DepthFunc::GREATER:
        return VkCompareOp::VK_COMPARE_OP_GREATER;
        break;
    case Horizon::DepthFunc::G_EQUAL:
        return VkCompareOp::VK_COMPARE_OP_GREATER_OR_EQUAL;
        break;
    case Horizon::DepthFunc::ALWAYS:
        return VkCompareOp::VK_COMPARE_OP_ALWAYS;
        break;
    default:
        return VkCompareOp::VK_COMPARE_OP_MAX_ENUM;
        break;
    }
}

} // namespace Horizon