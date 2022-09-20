#include "VulkanDescriptorSet.h"

namespace Horizon::RHI {

VulkanDescriptorSet::VulkanDescriptorSet(const VulkanRendererContext &context, ResourceUpdateFrequency frequency,
                                         const VulkanDescriptorSetManager &descriptor_set_manager,
                                         u64 layout_key) noexcept
    : DescriptorSet(frequency), m_context(context), m_descriptor_set_manager(descriptor_set_manager) {

    VkDescriptorSetLayout layout = m_descriptor_set_manager.FindLayout(layout_key);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_set_manager.m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, &set));
}

void VulkanDescriptorSet::SetResource(Buffer *buffer, u32 binding) {
    auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

    vk_buffer->buffer_info.buffer = vk_buffer->m_buffer;
    vk_buffer->buffer_info.offset = 0;
    vk_buffer->buffer_info.range = buffer->m_size;

    if (set == VK_NULL_HANDLE) {
        LOG_WARN("descriptor set not allocated")
        return;
    }

    auto &write = writes[binding];

    if (buffer->m_descriptor_type == DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    } else if (buffer->m_descriptor_type == DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstSet = set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pBufferInfo = &vk_buffer->buffer_info;
}

void VulkanDescriptorSet::SetResource(Texture *texture, u32 binding) {

    auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

    vk_texture->texture_info.imageLayout = util_to_vk_image_layout(texture->m_state);
    vk_texture->texture_info.imageView = vk_texture->m_image_view;

    if (set == VK_NULL_HANDLE) {
        LOG_WARN("descriptor set not allocated")
        return;
    }

    auto &write = writes[binding];

    if (texture->m_descriptor_type == DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    } else if (texture->m_descriptor_type == DescriptorType::DESCRIPTOR_TYPE_TEXTURE) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstSet = set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &vk_texture->texture_info;
}

void VulkanDescriptorSet::SetResource(Sampler *sampler, u32 binding) {
    auto vk_sampler = reinterpret_cast<VulkanSampler *>(sampler);
    vk_sampler->texture_info.sampler = vk_sampler->m_sampler;

    if (set == VK_NULL_HANDLE) {
        LOG_WARN("descriptor set not allocated")
        return;
    }

    auto &write = writes[binding];

    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstSet = set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &vk_sampler->texture_info;
}

void VulkanDescriptorSet::Update() {
    if (set == VK_NULL_HANDLE) {
        LOG_WARN("descriptor set not allocated")
    }

    std::vector<VkWriteDescriptorSet> vk_writes;

    vk_writes.reserve(writes.size());

    for (auto &val : writes) {
        if (val.dstSet != nullptr)
            vk_writes.emplace_back(val);
    }

    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(vk_writes.size()), vk_writes.data(), 0, nullptr);
}
} // namespace Horizon::RHI
