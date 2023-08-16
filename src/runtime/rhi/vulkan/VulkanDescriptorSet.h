#pragma once

#include <runtime/core/utils/definations.h>

#include <runtime/rhi/DescriptorSet.h>
#include <runtime/rhi/Enums.h>

#include <runtime/rhi/vulkan/VulkanBuffer.h>
#include <runtime/rhi/vulkan/VulkanSampler.h>
#include <runtime/rhi/vulkan/VulkanTexture.h>

namespace Horizon::Backend {

class VulkanDescriptorSet : public DescriptorSet {
  public:
    VulkanDescriptorSet(const VulkanRendererContext &context, ResourceUpdateFrequency frequency,
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

  public:
    const VulkanRendererContext &m_context{};
    const std::unordered_map<std::string, DescriptorDesc> &write_descs{}; // move to base class?
    std::vector<VkWriteDescriptorSet> writes{};
    std::unordered_map<std::string, std::vector<VkDescriptorImageInfo>> bindless_image_descriptors;
    std::unordered_map<std::string, std::vector<VkDescriptorBufferInfo>> bindless_buffer_descriptors;
    VkDescriptorSet m_set{};
};
} // namespace Horizon::Backend
