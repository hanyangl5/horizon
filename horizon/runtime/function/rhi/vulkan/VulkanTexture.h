#pragma once

#include "vk_mem_alloc.h"

#include <runtime/function/rhi/Texture.h>

namespace Horizon::RHI {
class VulkanTexture : public Texture {
  public:
    VulkanTexture(VmaAllocator allocator, const TextureCreateInfo &buffer_create_info) noexcept;
    virtual ~VulkanTexture() noexcept;

  public:
    VkImage m_image{};
    VmaAllocation m_allocation{};
    VmaAllocator m_allocator{};
};

} // namespace Horizon::RHI