#include "IndexBuffer.h"
#include "VulkanBuffer.h"

namespace Horizon {
	IndexBuffer::IndexBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, const std::vector<Index>& indices) :m_device(device)
	{
		m_indices_count = indices.size();
		VkDeviceSize buffer_size = sizeof(Index) * m_indices_count;

		// create stage buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(device->Get(), device->getPhysicalDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// upload cpu data
		void* data;
		vkMapMemory(m_device->Get(), stagingBufferMemory, 0, buffer_size, 0, &data);
		memcpy(data, indices.data(), buffer_size);
		vkUnmapMemory(m_device->Get(), stagingBufferMemory);

		// create gpu buffer
		vk_createBuffer(device->Get(), device->getPhysicalDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_index_buffer, m_index_buffer_memory);

		vk_copyBuffer(device, command_buffer, stagingBuffer, m_index_buffer, buffer_size);

		VkCommandBuffer cmdbuf = command_buffer->beginSingleTimeCommands();
		VkBufferMemoryBarrier bufferMemoryBarrier{};
		bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferMemoryBarrier.buffer = m_index_buffer;
		bufferMemoryBarrier.size = buffer_size;
		bufferMemoryBarrier.offset = 0;
		bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferMemoryBarrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
		vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
		command_buffer->endSingleTimeCommands(cmdbuf);

		vkDestroyBuffer(device->Get(), stagingBuffer, nullptr);
		vkFreeMemory(device->Get(), stagingBufferMemory, nullptr);
	}

	//VertexBuffer::VertexBuffer(const VertexBuffer&& rhs)
	//{
	//    m_vertex_buffer = rhs.m_vertex_buffer;
	//    m_vertex_buffer_memory = rhs.m_vertex_buffer_memory;
	//}
	//
	//VertexBuffer& VertexBuffer::operator=(VertexBuffer&& rhs)
	//{
	//    m_vertex_buffer = rhs.m_vertex_buffer;
	//    m_vertex_buffer_memory = rhs.m_vertex_buffer_memory;
	//    return *this;
	//}

	IndexBuffer::~IndexBuffer()
	{
		vkDestroyBuffer(m_device->Get(), m_index_buffer, nullptr);
		vkFreeMemory(m_device->Get(), m_index_buffer_memory, nullptr);
	}

	VkBuffer IndexBuffer::Get() const
	{
		return m_index_buffer;
	}

	u64 IndexBuffer::getIndicesCount() const
	{
		return m_indices_count;
	}

}