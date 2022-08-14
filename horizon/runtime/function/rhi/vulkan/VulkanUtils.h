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

inline bool operator==(const VkDescriptorSetLayoutCreateInfo &lhs,
                const VkDescriptorSetLayoutCreateInfo &rhs) {
    if (!lhs.bindingCount != rhs.bindingCount) {
        return false;
    }

    for (u32 i = 0; i < lhs.bindingCount; i++) {
        if (lhs.pBindings[i].binding != rhs.pBindings[i].binding) {
            return false;
        }
        if (lhs.pBindings[i].descriptorCount !=
            rhs.pBindings[i].descriptorCount) {
            return false;
        }
        if (lhs.pBindings[i].descriptorType !=
            rhs.pBindings[i].descriptorType) {
            return false;
        }
        if (lhs.pBindings[i].stageFlags != rhs.pBindings[i].stageFlags) {
            return false;
        }
    }
}


template <typename T>
inline void hash_combine(std::size_t &seed, const T &val) {
    seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct VkDescriptorSetLayoutCreateInfoHasher {
    inline std::size_t
    operator()(VkDescriptorSetLayoutCreateInfo const &layout) const {
        std::size_t seed = 0;
        hash_combine(seed, layout.bindingCount);
        for (u32 i = 0; i < layout.bindingCount; i++) {
            hash_combine(seed, layout.pBindings[i].binding);
            hash_combine(seed, layout.pBindings[i].descriptorCount);
            hash_combine(seed, layout.pBindings[i].stageFlags);
            hash_combine(seed, layout.pBindings[i].descriptorType);
        }
        return seed;
    }
};

} // namespace Horizon