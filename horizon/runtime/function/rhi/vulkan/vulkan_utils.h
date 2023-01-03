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

VkAccessFlags ToVkAccessFlags(ResourceState state) noexcept;

VkImageLayout ToVkImageLayout(ResourceState usage) noexcept;

VkImageUsageFlags ToVkImageUsage(DescriptorTypes types) noexcept;

VkPipelineStageFlags ToPipelineStageFlags(VkAccessFlags accessFlags,
                                                         CommandQueueType queueType) noexcept;

VkShaderStageFlags ToVkShaderStageFlags(u32 stage) noexcept;

VkImageType ToVkImageType(TextureType type) noexcept;

VkFormat ToVkImageFormat(TextureFormat format) noexcept;

VkImageAspectFlags ToVkAspectMaskFlags(VkFormat format, bool includeStencilBit) noexcept;

VkBufferUsageFlags ToVkBufferUsageFlags(DescriptorTypes usage, bool typed) noexcept;

VkFormat ToVkImageFormat(VertexAttribFormat format, u32 portions) noexcept;

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology t) noexcept;

VkFrontFace ToVkFrontFace(FrontFace front_face) noexcept;

VkCullModeFlagBits ToVkCullMode(CullMode cull_mode) noexcept;

VkPolygonMode ToVkPolygonMode(FillMode fill_mode) noexcept;

VkCompareOp ToVkCompareOp(DepthFunc depth_func) noexcept;

VkImageViewType ToVkImageViewType(TextureType type) noexcept;

VkAttachmentLoadOp ToVkLoadOp(RenderTargetLoadOp load_op) noexcept;

VkAttachmentStoreOp ToVkStoreOp(RenderTargetStoreOp store_op) noexcept;

} // namespace Horizon