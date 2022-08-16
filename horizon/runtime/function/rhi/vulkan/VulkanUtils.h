#pragma once

#include <array>
#include <functional>
#include <runtime/core/utils/definations.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

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

} // namespace Horizon