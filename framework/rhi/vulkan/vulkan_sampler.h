#pragma once

#include <rhi/enums.h>
#include <rhi/sampler.h>
#include <rhi/vulkan/vulkan_command_context.h>

namespace Horizon::Backend
{

class VulkanSampler : public Sampler
{
  public:
    VulkanSampler(const VulkanRendererContext &context, const SamplerDesc &desc) noexcept;
    virtual ~VulkanSampler() noexcept;

    VulkanSampler(const VulkanSampler &rhs) noexcept = delete;
    VulkanSampler &operator=(const VulkanSampler &rhs) noexcept = delete;
    VulkanSampler(VulkanSampler &&rhs) noexcept = delete;
    VulkanSampler &operator=(VulkanSampler &&rhs) noexcept = delete;
    VkDescriptorImageInfo *GetDescriptorImageInfo() noexcept;

  public:
    const VulkanRendererContext &m_context{};
    VkSampler m_sampler{};
    VkDescriptorImageInfo descriptor_image_info{};
};

} // namespace Horizon::Backend