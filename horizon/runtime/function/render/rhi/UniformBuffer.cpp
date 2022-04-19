#include "UniformBuffer.h"

namespace Horizon {

	UniformBuffer::UniformBuffer(std::shared_ptr<Device> device) : m_device(device)
	{
	}

	UniformBuffer::~UniformBuffer()
	{
		vkDestroyBuffer(m_device->Get(), m_uniform_buffer, nullptr);
		vkFreeMemory(m_device->Get(), m_uniform_buffer_memory, nullptr);
	}

	void UniformBuffer::update(void* Ub, u64 buffer_size)
	{
		if (!m_uniform_buffer) {
			vk_createBuffer(m_device->Get(), m_device->getPhysicalDevice(), buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniform_buffer, m_uniform_buffer_memory);
			m_size = buffer_size;
			bufferDescriptrInfo.buffer = m_uniform_buffer;
			bufferDescriptrInfo.offset = 0;
			bufferDescriptrInfo.range = buffer_size;
		}
		void* data;
		vkMapMemory(m_device->Get(), m_uniform_buffer_memory, 0, buffer_size, 0, &data);
		memcpy(data, Ub, buffer_size);
		vkUnmapMemory(m_device->Get(), m_uniform_buffer_memory);
	}

	VkBuffer UniformBuffer::Get() const
	{
		return m_uniform_buffer;
	}
	u64 UniformBuffer::size() const
	{
		return m_size;
	}
}