#include "UniformBuffer.h"

UniformBuffer::UniformBuffer(Device* device) : mDevice(device)
{
}

UniformBuffer::~UniformBuffer()
{
    vkDestroyBuffer(mDevice->get(), mUniformBuffer, nullptr);
    vkFreeMemory(mDevice->get(), mUniformBufferMemory, nullptr);
}

void UniformBuffer::update(void* ubo, u64 bufferSize)
{
    if (!mUniformBuffer) {
        createBuffer(mDevice->get(), mDevice->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mUniformBuffer, mUniformBufferMemory);
        mSize = bufferSize;
    }
    void* data;
    vkMapMemory(mDevice->get(), mUniformBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, &ubo, bufferSize);
    vkUnmapMemory(mDevice->get(), mUniformBufferMemory);
}

VkBuffer UniformBuffer::get() const
{
    return mUniformBuffer;
}
u64 UniformBuffer::size() const
{
    return mSize;
}
