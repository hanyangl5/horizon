#pragma once

#include <vulkan/vulkan.h>

#include <runtime/function/rhi/RenderContext.h>
#include "Device.h"
#include "VulkanBuffer.h"
#include "CommandBuffer.h"

namespace Horizon {
	class UniformBuffer : public DescriptorBase
	{
	public:
		UniformBuffer(std::shared_ptr<Device>);
		~UniformBuffer();
		void update(void* ub, u64 buffer_size);
		VkBuffer Get()const noexcept;
		u64 size()const noexcept;
	private:
		std::shared_ptr<Device> m_device = nullptr;
		VkBuffer m_uniform_buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_uniform_buffer_memory = VK_NULL_HANDLE;
		u64 m_size;
	};

}