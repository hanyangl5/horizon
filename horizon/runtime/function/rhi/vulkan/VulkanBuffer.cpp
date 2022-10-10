#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {

VulkanBuffer::VulkanBuffer(const VulkanRendererContext &context, const BufferCreateInfo &buffer_create_info,
                           MemoryFlag memory_flag) noexcept
    : Buffer(buffer_create_info), m_context(context) {
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = buffer_create_info.size;
    create_info.usage = util_to_vk_buffer_usage(buffer_create_info.descriptor_types, false);
    create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (memory_flag == MemoryFlag::CPU_VISABLE_MEMORY) {
        allocation_create_info.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    } else if (memory_flag == MemoryFlag::DEDICATE_GPU_MEMORY) {
        allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    CHECK_VK_RESULT(vmaCreateBuffer(m_context.vma_allocator, &create_info, &allocation_create_info, &m_buffer,
                                    &m_memory, &m_allocation_info));
}

VulkanBuffer::~VulkanBuffer() noexcept { vmaDestroyBuffer(m_context.vma_allocator, m_buffer, m_memory); }

VkDescriptorBufferInfo *VulkanBuffer::GetDescriptorBufferInfo(u32 offset, u32 size) noexcept {

    buffer_info = {m_buffer, offset, m_size};
    return &buffer_info;
}

} // namespace Horizon::RHI
