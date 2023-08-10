#include "VertexBuffer.h"
#include "VulkanBuffer.h"

namespace Horizon {
VertexBuffer::VertexBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer,
                           const std::vector<Vertex> &vertices)
    : m_device(device) {
    m_vertices_count = vertices.size();
    VkDeviceSize buffer_size = sizeof(vertices[0]) * m_vertices_count;

    // create stage buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vk_createBuffer(device->Get(), device->getPhysicalDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                    stagingBufferMemory);

    // upload cpu data
    void *data;
    vkMapMemory(m_device->Get(), stagingBufferMemory, 0, buffer_size, 0, &data);
    memcpy(data, vertices.data(), buffer_size);
    vkUnmapMemory(m_device->Get(), stagingBufferMemory);

    // create actual vertex buffer
    vk_createBuffer(device->Get(), device->getPhysicalDevice(), buffer_size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertex_buffer, m_vertex_buffer_memory);

    vk_copyBuffer(device, command_buffer, stagingBuffer, m_vertex_buffer, buffer_size);

    VkCommandBuffer cmdbuf = command_buffer->beginSingleTimeCommands();
    VkBufferMemoryBarrier bufferMemoryBarrier{};
    bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferMemoryBarrier.buffer = m_vertex_buffer;
    bufferMemoryBarrier.size = buffer_size;
    bufferMemoryBarrier.offset = 0;
    bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
                         &bufferMemoryBarrier, 0, nullptr);
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

VertexBuffer::~VertexBuffer() {
    vkDestroyBuffer(m_device->Get(), m_vertex_buffer, nullptr);
    vkFreeMemory(m_device->Get(), m_vertex_buffer_memory, nullptr);
}

VkBuffer VertexBuffer::Get() const noexcept { return m_vertex_buffer; }

u64 VertexBuffer::getVerticesCount() const noexcept { return m_vertices_count; }
} // namespace Horizon