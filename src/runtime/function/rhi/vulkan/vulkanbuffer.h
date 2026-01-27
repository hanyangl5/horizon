#include <vulkan/vulkan.hpp>

#include "CommandBuffer.h"
#include "Device.h"
#include <runtime/function/rhi/RenderContext.h>

namespace Horizon {

void vk_createBuffer(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &buffer_memory);

void vk_copyBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, VkBuffer srcBuffer,
                   VkBuffer dstBuffer, VkDeviceSize size);

void vk_copyBufferToImage(std::shared_ptr<CommandBuffer> command_buffer, VkBuffer buffer, VkImage image, u32 width,
                          u32 height);
} // namespace Horizon