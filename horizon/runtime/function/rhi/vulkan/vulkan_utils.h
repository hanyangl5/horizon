#pragma once


#include <functional>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/rhi_utils.h>

namespace Horizon {

struct VulkanRendererContext {
    VkInstance instance{};
    VkPhysicalDevice active_gpu{};
    // VkPhysicalDeviceProperties* vk_active_gpu_properties;
    VkDevice device{};
    VmaAllocator vma_allocator{};
    Container::FixedArray<u32, 3> command_queue_familiy_indices{};
    Container::FixedArray<VkQueue, 3> command_queues{};
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

} // namespace Horizon