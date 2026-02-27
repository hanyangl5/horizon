#include "vulkan_descriptor_set.h"

#include <core/log.h>

namespace Horizon::Backend
{

VulkanDescriptorSet::VulkanDescriptorSet(const VulkanRendererContext &context, u32 set_number,
                                         const std::unordered_map<std::string, DescriptorDesc> &write_descs,
                                         VkDescriptorSet set) noexcept
    : DescriptorSet(set_number), m_context(context), write_descs(write_descs), m_set(set)
{
}

void VulkanDescriptorSet::SetResource(Buffer *buffer, const std::string &resource_name)
{
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end())
    {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }

    auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

    auto &buffer_info = m_buffer_descriptors[resource_name];
    buffer_info = *vk_buffer->GetDescriptorBufferInfo(0, (u32)buffer->m_size);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = res->second.vk_binding;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type);
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.pBufferInfo = &buffer_info;

    m_pending_writes[resource_name] = write;
    m_dirty = true;
}

void VulkanDescriptorSet::SetResource(Texture *texture, const std::string &resource_name)
{
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end())
    {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }
    auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

    auto &image_info = m_image_descriptors[resource_name];
    image_info = *vk_texture->GetDescriptorImageInfo(res->second.type);

    VkWriteDescriptorSet write{};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = res->second.vk_binding;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type);
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.pImageInfo = &image_info;
    m_pending_writes[resource_name] = write;
    m_dirty = true;
}

void VulkanDescriptorSet::SetResource(Sampler *sampler, const std::string &resource_name)
{
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end())
    {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }
    auto vk_sampler = reinterpret_cast<VulkanSampler *>(sampler);

    auto &image_info = m_image_descriptors[resource_name];
    image_info = *vk_sampler->GetDescriptorImageInfo();

    VkWriteDescriptorSet write{};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = res->second.vk_binding;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type); // sampler
    write.descriptorCount = 1;
    write.dstSet = m_set;
    write.dstArrayElement = 0;
    write.pImageInfo = &image_info;
    m_pending_writes[resource_name] = write;
    m_dirty = true;
}

void VulkanDescriptorSet::SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name)
{
    // Bindless resources are typically in a specific set (e.g., set 4)
    // This can be checked if needed, but for now we just allow it
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end())
    {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }

    auto &buffer_descriptors = bindless_buffer_descriptors[resource_name];
    buffer_descriptors.clear();
    buffer_descriptors.reserve(resource.size());

    for (auto &buffer : resource)
    {
        auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

        buffer_descriptors.push_back(*vk_buffer->GetDescriptorBufferInfo(0, (u32)buffer->m_size));
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = res->second.vk_binding;
    write.dstArrayElement = 0;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type);

    write.descriptorCount = static_cast<uint32_t>(resource.size());
    write.pBufferInfo = buffer_descriptors.data();
    write.dstSet = m_set;
    m_pending_writes[resource_name] = write;
    m_dirty = true;
}

void VulkanDescriptorSet::SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name)
{
    // Bindless resources are typically in a specific set (e.g., set 4)
    // This can be checked if needed, but for now we just allow it
    auto res = write_descs.find(resource_name);
    if (res == write_descs.end())
    {
        LOG_ERROR("resource {} is not declared in this descriptorset", resource_name);
        return;
    }

    auto &bindless_texture_descriptors = bindless_image_descriptors[resource_name];
    bindless_texture_descriptors.clear();
    bindless_texture_descriptors.reserve(resource.size());

    for (auto &texture : resource)
    {
        auto vk_texture = reinterpret_cast<VulkanTexture *>(texture);

        bindless_texture_descriptors.push_back(*vk_texture->GetDescriptorImageInfo(res->second.type));
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = res->second.vk_binding;
    write.dstArrayElement = 0;
    write.descriptorType = util_to_vk_descriptor_type(res->second.type);

    write.descriptorCount = static_cast<uint32_t>(resource.size());
    write.pBufferInfo = nullptr;
    write.dstSet = m_set;
    write.pImageInfo = bindless_texture_descriptors.data();
    m_pending_writes[resource_name] = write;
    m_dirty = true;
}

void VulkanDescriptorSet::Update()
{
    if (!m_dirty || m_pending_writes.empty())
    {
        return;
    }

    m_write_batch.clear();
    m_write_batch.reserve(m_pending_writes.size());
    for (const auto &[name, write] : m_pending_writes)
    {
        (void)name;
        m_write_batch.push_back(write);
    }

    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(m_write_batch.size()), m_write_batch.data(), 0, nullptr);
    m_dirty = false;
}

bool VulkanDescriptorSet::IsDirty() const
{
    return m_dirty;
}
} // namespace Horizon::Backend
