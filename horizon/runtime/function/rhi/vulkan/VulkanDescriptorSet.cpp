#include "VulkanDescriptorSet.h"

namespace Horizon::RHI {

VulkanDescriptorSet::VulkanDescriptorSet(const VulkanRendererContext &context, ResourceUpdateFrequency frequency,
                                         const std::unordered_map<std::string, DescriptorDesc> &write_descs,
                                         VkDescriptorSet set) noexcept
    : DescriptorSet(frequency), m_context(context), m_set(set), write_descs(write_descs) {}

void VulkanDescriptorSet::SetResource(Buffer *buffer, const std::string &resource_name) {
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

void VulkanDescriptorSet::SetResource(Texture *texture, const std::string &resource_name) {
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

void VulkanDescriptorSet::SetResource(Sampler *sampler, const std::string &resource_name) {
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

void VulkanDescriptorSet::Update() {
    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}
} // namespace Horizon::RHI
