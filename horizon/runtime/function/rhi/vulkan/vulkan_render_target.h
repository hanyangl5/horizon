#pragma once

#include <runtime/function/rhi/render_target.h>
#include <runtime/function/rhi/vulkan/vulkan_command_context.h>
#include <runtime/function/rhi/vulkan/vulkan_render_target.h>

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