#pragma once

#include <array>
#include <functional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon {
struct VulkanRendererContext {
    VkInstance instance;
    VkPhysicalDevice active_gpu;
    // VkPhysicalDeviceProperties* vk_active_gpu_properties;
    VkDevice device;
    VmaAllocator vma_allocator;
    std::array<u32, 3> command_queue_familiy_indices;
    std::array<VkQueue, 3> command_queues;
    std::array<VkFence, 3> fences;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR optimal_surface_format;
    VkSwapchainKHR swap_chain;
    std::vector<VkImage> swap_chain_images;
    std::vector<VkImageView> swap_chain_image_views;
};

VkPipelineBindPoint ToVkPipelineBindPoint(PipelineType type) noexcept;

VkShaderStageFlagBits ToVkShaderStageBit(ShaderType type) noexcept;

VkAccessFlags util_to_vk_access_flags(ResourceState state) noexcept;

VkImageLayout util_to_vk_image_layout(ResourceState usage) noexcept;

VkImageUsageFlags util_to_vk_image_usage(DescriptorType usage) noexcept;

VkPipelineStageFlags util_determine_pipeline_stage_flags(VkAccessFlags accessFlags,
                                                         CommandQueueType queueType) noexcept;

VkShaderStageFlags ToVkShaderStageFlags(u32 stage) noexcept;

VkImageType ToVkImageType(TextureType type) noexcept;

VkFormat ToVkImageFormat(TextureFormat format) noexcept;

VkImageAspectFlags ToVkAspectMaskFlags(VkFormat format, bool includeStencilBit) noexcept;

VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorType usage, bool typed) noexcept;

VkFormat ToVkImageFormat(VertexAttribFormat format, u32 portions) noexcept;

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology t) noexcept;

VkFrontFace ToVkFrontFace(FrontFace front_face) noexcept;

VkCullModeFlagBits ToVkCullMode(CullMode cull_mode) noexcept;

VkPolygonMode ToVkPolygonMode(FillMode fill_mode) noexcept;

VkCompareOp ToVkCompareOp(DepthFunc depth_func) noexcept;

} // namespace Horizon