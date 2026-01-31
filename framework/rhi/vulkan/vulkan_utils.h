#pragma once

#include <functional>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <core/definations.h>
#include <rhi/enums.h>

namespace Horizon {

struct VulkanRendererContext {
    VkInstance instance{};
    VkPhysicalDevice active_gpu{};
    VkDevice device{};
    VmaAllocator vma_allocator{};
    std::array<u32, 3> command_queue_familiy_indices{};
    std::array<VkQueue, 3> command_queues{};
    VkQueryPool gpu_query_pool{};
    float timestampPeriod;
};

VkPipelineBindPoint ToVkPipelineBindPoint(PipelineType type) noexcept;

VkShaderStageFlagBits ToVkShaderStageBit(ShaderType type) noexcept;

VkAccessFlags util_to_vk_access_flags(ResourceState state) noexcept;

VkImageLayout util_to_vk_image_layout(ResourceState usage) noexcept;

VkImageUsageFlags util_to_vk_image_usage(DescriptorTypes types) noexcept;

VkPipelineStageFlags util_determine_pipeline_stage_flags(VkAccessFlags accessFlags,
                                                         CommandQueueType queueType) noexcept;

VkShaderStageFlags ToVkShaderStageFlags(u32 stage) noexcept;

VkImageType ToVkImageType(TextureType type) noexcept;

VkFormat ToVkImageFormat(TextureFormat format) noexcept;

VkImageAspectFlags ToVkAspectMaskFlags(VkFormat format, bool includeStencilBit) noexcept;

VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorTypes usage, bool typed) noexcept;

VkFormat ToVkImageFormat(VertexAttribFormat format, u32 portions) noexcept;

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology t) noexcept;

VkFrontFace ToVkFrontFace(FrontFace front_face) noexcept;

VkCullModeFlagBits ToVkCullMode(CullMode cull_mode) noexcept;

VkPolygonMode ToVkPolygonMode(FillMode fill_mode) noexcept;

VkCompareOp ToVkCompareOp(DepthFunc depth_func) noexcept;

inline VkImageViewType ToVkImageViewType(TextureType type) noexcept {
    switch (type) {
    case Horizon::TextureType::TEXTURE_TYPE_1D:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;
        break;
    case Horizon::TextureType::TEXTURE_TYPE_2D:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
        break;
    case Horizon::TextureType::TEXTURE_TYPE_3D:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;

        break;
    case Horizon::TextureType::TEXTURE_TYPE_CUBE:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    default:
        assert(false);
        return {};
        break;
    }
}

VkAttachmentLoadOp ToVkLoadOp(RenderTargetLoadOp load_op);

VkAttachmentStoreOp ToVkStoreOp(RenderTargetStoreOp store_op);


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

DescriptorType vk_to_descriptor_type(VkDescriptorType vk) noexcept;

inline void CheckVulkanResult(VkResult _res, const char *func_name, int line)  noexcept {
    if (_res != VK_SUCCESS) {
        LOG_ERROR("[function: {}], [line: {}], vulkan error", func_name, line);
    }
    assert(_res == VK_SUCCESS);
  }

#define CHECK_VK_RESULT(res) CheckVulkanResult(res, __FUNCTION__, __LINE__);



} // namespace Horizon