#pragma once

#include <string>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Sampler.h>
#include <runtime/function/rhi/vulkan/VulkanCommandContext.h>

namespace Horizon::RHI {

class VulkanSampler : public Sampler {
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

} // namespace Horizon::RHI