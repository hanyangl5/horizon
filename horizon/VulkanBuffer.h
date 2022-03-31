#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {

	void vk_createBuffer(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	
	void vk_copyBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandbuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	
	void vk_copyBufferToImage(std::shared_ptr<CommandBuffer> commandbuffer, VkBuffer buffer, VkImage image, u32 width, u32 height);
}