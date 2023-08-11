#include "VulkanDescriptorSet.h"

namespace Horizon::Backend {

VulkanDescriptorSet::VulkanDescriptorSet(const VulkanRendererContext &context, ResourceUpdateFrequency frequency,
                                         const Container::HashMap<Container::String, DescriptorDesc> &write_descs,
                                         VkDescriptorSet set) noexcept
    : DescriptorSet(frequency), m_context(context), m_set(set), write_descs(write_descs) {}

void VulkanDescriptorSet::SetResource(Buffer *buffer, const Container::String &resource_name) {
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end()) {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }

    auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = res->second.vk_binding;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type);
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pBufferInfo = vk_buffer->GetDescriptorBufferInfo(0, buffer->m_size);

    writes.push_back(write);
}

void VulkanDescriptorSet::SetResource(Texture *texture, const Container::String &resource_name) {
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end()) {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }
    auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

    VkWriteDescriptorSet write;

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = res->second.vk_binding;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type);
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.pImageInfo = vk_texture->GetDescriptorImageInfo(res->second.type);
    writes.push_back(write);
}

void VulkanDescriptorSet::SetResource(Sampler *sampler, const Container::String &resource_name) {
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end()) {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }
    auto vk_sampler = reinterpret_cast<VulkanSampler *>(sampler);

    VkWriteDescriptorSet write{};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = res->second.vk_binding;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type); // sampler
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.pImageInfo = vk_sampler->GetDescriptorImageInfo();
    writes.push_back(write);
}

void VulkanDescriptorSet::SetBindlessResource(Container::Array<Buffer *> &resource,
                                              const Container::String &resource_name) {
    assert(update_frequency == ResourceUpdateFrequency::BINDLESS);
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end()) {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }

    auto &buffer_descriptors = bindless_buffer_descriptors[resource_name];

    for (auto &buffer : resource) {
        auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

        buffer_descriptors.push_back(*vk_buffer->GetDescriptorBufferInfo(0, buffer->m_size));
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = res->second.vk_binding;
    write.dstArrayElement = 0;
    write.descriptorType = write.descriptorType = util_to_vk_descriptor_type(res->second.type);

    write.descriptorCount = static_cast<uint32_t>(resource.size());
    write.pBufferInfo = buffer_descriptors.data();
    write.dstSet = m_set;
    writes.push_back(write);
}

void VulkanDescriptorSet::SetBindlessResource(Container::Array<Texture *> &resource,
                                              const Container::String &resource_name) {

    assert(update_frequency == ResourceUpdateFrequency::BINDLESS);
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end()) {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }

    auto &texture_descriptors = bindless_image_descriptors[resource_name];

    for (auto &texture : resource) {
        auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

        texture_descriptors.push_back(*vk_texture->GetDescriptorImageInfo(res->second.type));
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = res->second.vk_binding;
    write.dstArrayElement = 0;
    write.descriptorType = write.descriptorType = util_to_vk_descriptor_type(res->second.type);

    write.descriptorCount = static_cast<uint32_t>(resource.size());
    write.pBufferInfo = 0;
    write.dstSet = m_set;
    write.pImageInfo = texture_descriptors.data();
    writes.push_back(write);
}
void VulkanDescriptorSet::SetBindlessResource(Container::Array<Sampler *> &resource,
                                              const Container::String &resource_name) {}

void VulkanDescriptorSet::Update() {
    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}
} // namespace Horizon::Backend
