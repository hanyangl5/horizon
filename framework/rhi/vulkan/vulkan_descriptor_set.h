#pragma once

#include <core/definations.h>

#include <rhi/descriptor_set.h>
#include <rhi/enums.h>

#include <rhi/vulkan/vulkan_buffer.h>
#include <rhi/vulkan/vulkan_sampler.h>
#include <rhi/vulkan/vulkan_texture.h>

namespace Horizon::Backend
{

class VulkanDescriptorSet : public DescriptorSet
{
  public:
    VulkanDescriptorSet(const VulkanRendererContext &context, u32 set_number,
                        const std::unordered_map<std::string, DescriptorDesc> &write_descs,
                        VkDescriptorSet set) noexcept;
    virtual ~VulkanDescriptorSet() noexcept {};

    VulkanDescriptorSet(const VulkanDescriptorSet &rhs) noexcept = delete;
    VulkanDescriptorSet &operator=(const VulkanDescriptorSet &rhs) noexcept = delete;
    VulkanDescriptorSet(VulkanDescriptorSet &&rhs) noexcept = delete;
    VulkanDescriptorSet &operator=(VulkanDescriptorSet &&rhs) noexcept = delete;

  public:
    void SetResource(Buffer *resource, const std::string &resource_name) override;
    void SetResource(Texture *resource, const std::string &resource_name) override;
    void SetResource(Sampler *resource, const std::string &resource_name) override;

    void SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name) override;
    void SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name) override;

    void Update() override;
    bool IsDirty() const override;

  public:
    const VulkanRendererContext &m_context{};
    const std::unordered_map<std::string, DescriptorDesc> &write_descs{}; // move to base class?
    std::unordered_map<std::string, VkWriteDescriptorSet> m_pending_writes{};
    std::vector<VkWriteDescriptorSet> m_write_batch{};
    std::unordered_map<std::string, VkDescriptorBufferInfo> m_buffer_descriptors{};
    std::unordered_map<std::string, VkDescriptorImageInfo> m_image_descriptors{};
    std::unordered_map<std::string, std::vector<VkDescriptorImageInfo>> bindless_image_descriptors;
    std::unordered_map<std::string, std::vector<VkDescriptorBufferInfo>> bindless_buffer_descriptors;
    bool m_dirty{false};
    VkDescriptorSet m_set{};
};
} // namespace Horizon::Backend
