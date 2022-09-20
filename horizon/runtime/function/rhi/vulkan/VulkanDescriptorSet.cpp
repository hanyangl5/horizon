#include "VulkanDescriptorSet.h"

namespace Horizon::RHI {

VulkanDescriptorSet::VulkanDescriptorSet(const VulkanRendererContext &context, ResourceUpdateFrequency frequency, VkDescriptorSet set) noexcept
    : DescriptorSet(frequency), m_context(context), m_set(set) {
}

void VulkanDescriptorSet::SetResource(Buffer *buffer, u32 binding) {
    auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

    vk_buffer->buffer_info.buffer = vk_buffer->m_buffer;
    vk_buffer->buffer_info.offset = 0;
    vk_buffer->buffer_info.range = buffer->m_size;

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
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pBufferInfo = &vk_buffer->buffer_info;
}

void VulkanDescriptorSet::SetResource(Texture *texture, u32 binding) {

    auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

    vk_texture->texture_info.imageLayout = util_to_vk_image_layout(texture->m_state);
    vk_texture->texture_info.imageView = vk_texture->m_image_view;

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
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &vk_texture->texture_info;
}

void VulkanDescriptorSet::SetResource(Sampler *sampler, u32 binding) {
    auto vk_sampler = reinterpret_cast<VulkanSampler *>(sampler);
    vk_sampler->texture_info.sampler = vk_sampler->m_sampler;

    auto &write = writes[binding];

    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &vk_sampler->texture_info;
}

void VulkanDescriptorSet::Update() {

    std::vector<VkWriteDescriptorSet> valid_writes;

    for (auto &val : writes) {
        if (val.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
            valid_writes.emplace_back(val);
    }

    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(valid_writes.size()), valid_writes.data(), 0, nullptr);
}
} // namespace Horizon::RHI
