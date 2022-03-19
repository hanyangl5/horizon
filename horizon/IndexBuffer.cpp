#include "IndexBuffer.h"
#include "VulkanBuffer.h"

namespace Horizon {
	IndexBuffer::IndexBuffer(Device* device, CommandBuffer* commandBuffer, const std::vector<Index>& indices) :mDevice(device)
	{
		mIndicesCount = indices.size();
		VkDeviceSize bufferSize = sizeof(Index) * mIndicesCount;

		// create stage buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(device->get(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// upload cpu data
		void* data;
		vkMapMemory(mDevice->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), bufferSize);
		vkUnmapMemory(mDevice->get(), stagingBufferMemory);

		// create gpu buffer
		vk_createBuffer(device->get(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mIndexBuffer, mIndexBufferMemory);

		vk_copyBuffer(device, commandBuffer, stagingBuffer, mIndexBuffer, bufferSize);

		VkCommandBuffer cmdbuf = commandBuffer->beginSingleTimeCommands();
		VkBufferMemoryBarrier bufferMemoryBarrier{};
		bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferMemoryBarrier.buffer = mIndexBuffer;
		bufferMemoryBarrier.size = bufferSize;
		bufferMemoryBarrier.offset = 0;
		bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferMemoryBarrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
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

	IndexBuffer::~IndexBuffer()
	{
		vkDestroyBuffer(mDevice->get(), mIndexBuffer, nullptr);
		vkFreeMemory(mDevice->get(), mIndexBufferMemory, nullptr);
	}

	VkBuffer IndexBuffer::get() const
	{
		return mIndexBuffer;
	}

	u64 IndexBuffer::getIndicesCount() const
	{
		return mIndicesCount;
	}

}