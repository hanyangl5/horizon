#include "IndexBuffer.h"
#include "VulkanBuffer.h"
IndexBuffer::IndexBuffer(Device* device, CommandBuffer* commandBuffer, const std::vector<Index>& indices) :mDevice(device)
{
    mIndicesCount = indices.size();
    VkDeviceSize bufferSize = sizeof(Index) * mIndicesCount;

    // create stage buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vkCreateBuffer(device->get(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // upload cpu data
    void* data;
    vkMapMemory(mDevice->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(mDevice->get(), stagingBufferMemory);

    // create actual vertex buffer
    vkCreateBuffer(device->get(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mIndexBuffer, mIndexBufferMemory);

    copyBuffer(device, commandBuffer, stagingBuffer, mIndexBuffer, bufferSize);

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
