#pragma once

#include <array>

#include <runtime/core/utils/Definations.h>

#include <runtime/function/rhi/DescriptorSet.h>
#include <runtime/function/rhi/RHIUtils.h>

#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanSampler.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>

namespace Horizon::RHI {

class VulkanDescriptorSet : public DescriptorSet {
  public:
    VulkanDescriptorSet(const VulkanRendererContext& context, ResourceUpdateFrequency frequency, VkDescriptorSet* pset) noexcept;
    virtual ~VulkanDescriptorSet() noexcept {};

    VulkanDescriptorSet(const VulkanDescriptorSet &rhs) noexcept = delete;
    VulkanDescriptorSet &operator=(const VulkanDescriptorSet &rhs) noexcept = delete;
    VulkanDescriptorSet(VulkanDescriptorSet &&rhs) noexcept = delete;
    VulkanDescriptorSet &operator=(VulkanDescriptorSet &&rhs) noexcept = delete;

  public:
    void SetResource(Buffer *resource, u32 binding) override;
    void SetResource(Texture *resource, u32 binding) override;
    void SetResource(Sampler *resource, u32 binding) override;

    void Update() override;

  public:
    const VulkanRendererContext &m_context;
    VkDescriptorSet m_set{};
    std::array<VkWriteDescriptorSet, MAX_BINDING_PER_DESCRIPTOR_SET> writes{};
};
} // namespace Horizon::RHI
