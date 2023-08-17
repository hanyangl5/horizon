#pragma once

#include <runtime/rhi/RenderTarget.h>
#include <runtime/rhi/vulkan/VulkanCommandContext.h>
#include <runtime/rhi/vulkan/VulkanRenderTarget.h>

namespace Horizon::Backend {

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
    const VulkanRendererContext &m_context{};
};

} // namespace Horizon::Backend