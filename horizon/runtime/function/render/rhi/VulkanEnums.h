#pragma once

#include <vulkan/vulkan.hpp>

#include <runtime/core/math/Math.h>
#include <runtime/core/log/Log.h>

namespace Horizon {

	inline u32 FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		LOG_ERROR("failed to find suitable memory type");
		return 0;
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