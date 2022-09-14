#pragma once

#include "vk_mem_alloc.h"

#include <runtime/function/rhi/RenderTarget.h>
#include <runtime/function/rhi/vulkan/VulkanCommandContext.h>
#include <runtime/function/rhi/vulkan/VulkanRenderTarget.h>

namespace Horizon::RHI {

class VulkanRenderTarget : public RenderTarget {
  public:
    VulkanRenderTarget(const VulkanRendererContext &context,
                       const RenderTargetCreateInfo &render_target_create_info) noexcept;
    virtual ~VulkanRenderTarget() noexcept;
    VulkanRenderTarget(const VulkanRenderTarget &rhs) noexcept = delete;
    VulkanRenderTarget &operator=(const VulkanRenderTarget &rhs) noexcept = delete;
    VulkanRenderTarget(VulkanRenderTarget &&rhs) noexcept = delete;
    VulkanRenderTarget &operator=(VulkanRenderTarget &&rhs) noexcept = delete;

  public:
    const VulkanRendererContext &m_context;
};

} // namespace Horizon::RHI