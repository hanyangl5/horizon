#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"
void createBuffer(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void copyBuffer(std::shared_ptr<Device> device,VkCommandPool cmdpool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
