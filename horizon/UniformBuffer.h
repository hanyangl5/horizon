#pragma once

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "VulkanBuffer.h"
#include "CommandBuffer.h"

namespace Horizon {
	class UniformBuffer : public DescriptorBase
	{
	public:
		UniformBuffer(std::shared_ptr<Device>);
		~UniformBuffer();
		void update(void* Ub, u64 bufferSize);
		VkBuffer get()const;
		u64 size()const;
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		VkBuffer mUniformBuffer = VK_NULL_HANDLE;
		VkDeviceMemory mUniformBufferMemory = VK_NULL_HANDLE;
		u64 mSize;
	};

}