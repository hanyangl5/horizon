#pragma once

#include <vulkan/vulkan.hpp>

#include <runtime/core/math/Math.h>

namespace Horizon {

	inline u32 findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}



	struct DescriptorBase
	{
		//DescriptorType type;
		VkDescriptorImageInfo imageDescriptorInfo{};
		VkDescriptorBufferInfo bufferDescriptrInfo{};
	};

	//struct BarrierDesc {
	//	std::vector<>
	//};
	//void insertBarrier(std::shared_ptr<Commandbuffer> command_buffer) {
	//	vkCmdPipelineBarrier()
	//}
}