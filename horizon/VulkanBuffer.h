#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {

	void vk_createBuffer(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	
	void vk_copyBuffer(Device* device, CommandBuffer* commandbuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	
	void vk_copyBufferToImage(CommandBuffer* commandbuffer, VkBuffer buffer, VkImage image, u32 width, u32 height);
}