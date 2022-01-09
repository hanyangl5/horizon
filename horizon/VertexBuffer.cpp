#include "VertexBuffer.h"
#include "VulkanBuffer.h"

VertexBuffer::VertexBuffer(std::shared_ptr<Device> device,VkCommandPool cmdpool, const std::vector<Vertex>& vertices) :mDevice(device)
{
    mVerticesCount = vertices.size();
    VkDeviceSize bufferSize = sizeof(vertices[0]) * mVerticesCount;

    // create stage buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device->get(),device->getPhysicalDevice(),bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // upload cpu data
    void* data;
    vkMapMemory(mDevice->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(mDevice->get(), stagingBufferMemory);

    // create actual vertex buffer
    createBuffer(device->get(), device->getPhysicalDevice(),bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mVertexBuffer, mVertexBufferMemory);

    copyBuffer(device,cmdpool,stagingBuffer, mVertexBuffer, bufferSize);

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
