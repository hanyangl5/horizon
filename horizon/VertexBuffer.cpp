#include "VertexBuffer.h"


VertexBuffer::VertexBuffer(std::shared_ptr<Device> device, const std::vector<Vertex>& vertices) :mDevice(device)
{

    mVerticesCount = vertices.size();
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(vertices[0]) * vertices.size();
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vkCreateBuffer(mDevice->get(), &bufferInfo, nullptr, &mVertexBuffer);


	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(mDevice->get(), mVertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(mDevice->getPhysicalDevice(),memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(mDevice->get(), &allocInfo, nullptr, &mVertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(mDevice->get(), mVertexBuffer, mVertexBufferMemory, 0);

    void* data;
    vkMapMemory(mDevice->get(), mVertexBufferMemory, 0, bufferInfo.size, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferInfo.size);
    vkUnmapMemory(mDevice->get(), mVertexBufferMemory);
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
