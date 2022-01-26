#pragma once
#include "utils.h"
#include <vulkan/vulkan.hpp>
#include "VulkanBuffer.h"
#include "Device.h"
class UniformBuffer
{
private:
    std::shared_ptr<Device> mDevice;
	VkBuffer mUniformBuffer = VK_NULL_HANDLE;
	VkDeviceMemory mUniformBufferMemory = VK_NULL_HANDLE;
	u64 mSize;
public:
	UniformBuffer(std::shared_ptr<Device> device);
	~UniformBuffer();
	void update(void* ubo, u64 bufferSize);
	VkBuffer get()const;
	u64 size()const;
};

