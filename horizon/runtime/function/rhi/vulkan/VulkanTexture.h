#pragma once

#include "vk_mem_alloc.h"
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {
class VulkanTexture : public Texture {
  public:
    VulkanTexture(const VulkanRendererContext& context, const TextureCreateInfo &buffer_create_info) noexcept;
    virtual ~VulkanTexture() noexcept;
    VulkanTexture(const VulkanTexture &rhs) noexcept = delete;
    VulkanTexture &operator=(const VulkanTexture &rhs) noexcept = delete;
    VulkanTexture(VulkanTexture &&rhs) noexcept = delete;
    VulkanTexture &operator=(VulkanTexture &&rhs) noexcept = delete;

  public:
    const VulkanRendererContext &m_context;
    VkImage m_image{};
    VkImageView m_image_view{};
    VmaAllocation m_allocation{};
};

} // namespace Horizon::RHI