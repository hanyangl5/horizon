#pragma once
#include "utils.h"
#include <vulkan/vulkan.hpp>
#include "VulkanBuffer.h"
#include "Device.h"
#include "CommandBuffer.h"

class UniformBuffer: public DescriptorBase
{
public:
	UniformBuffer(Device* device);
	~UniformBuffer();
	void update(void* Ub, u64 bufferSize);
	VkBuffer get()const;
	u64 size()const;
private:
	Device* mDevice = nullptr;
	VkBuffer mUniformBuffer = VK_NULL_HANDLE;
	VkDeviceMemory mUniformBufferMemory = VK_NULL_HANDLE;
	u64 mSize;
};

