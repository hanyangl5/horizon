#include "VertexBuffer.h"
#include "VulkanBuffer.h"

namespace Horizon {
	VertexBuffer::VertexBuffer(Device* device, CommandBuffer* commandBuffer, const std::vector<Vertex>& vertices) :mDevice(device)
	{
		mVerticesCount = vertices.size();
		VkDeviceSize bufferSize = sizeof(vertices[0]) * mVerticesCount;

		// create stage buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(device->get(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// upload cpu data
		void* data;
		vkMapMemory(mDevice->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), bufferSize);
		vkUnmapMemory(mDevice->get(), stagingBufferMemory);

		// create actual vertex buffer
		vk_createBuffer(device->get(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mVertexBuffer, mVertexBufferMemory);

		vk_copyBuffer(device, commandBuffer, stagingBuffer, mVertexBuffer, bufferSize);

		VkCommandBuffer cmdbuf = commandBuffer->beginSingleTimeCommands();
		VkBufferMemoryBarrier bufferMemoryBarrier{};
		bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferMemoryBarrier.buffer = mVertexBuffer;
		bufferMemoryBarrier.size = bufferSize;
		bufferMemoryBarrier.offset = 0;
		bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
		commandBuffer->endSingleTimeCommands(cmdbuf);

		vkDestroyBuffer(device->get(), stagingBuffer, nullptr);
		vkFreeMemory(device->get(), stagingBufferMemory, nullptr);
	}

	//VertexBuffer::VertexBuffer(const VertexBuffer&& rhs)
	//{
	//    mVertexBuffer = rhs.mVertexBuffer;
	//    mVertexBufferMemory = rhs.mVertexBufferMemory;
	//}
	//
	//VertexBuffer& VertexBuffer::operator=(VertexBuffer&& rhs)
	//{
	//    mVertexBuffer = rhs.mVertexBuffer;
	//    mVertexBufferMemory = rhs.mVertexBufferMemory;
	//    return *this;
	//}

	VertexBuffer::~VertexBuffer()
	{
		vkDestroyBuffer(mDevice->get(), mVertexBuffer, nullptr);
		vkFreeMemory(mDevice->get(), mVertexBufferMemory, nullptr);
	}

	VkBuffer VertexBuffer::get() const
	{
		return mVertexBuffer;
	}

	u64 VertexBuffer::getVerticesCount() const
	{
		return mVerticesCount;
	}
}