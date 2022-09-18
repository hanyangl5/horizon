#pragma once

#include <string>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/vulkan/VulkanCommandContext.h>

namespace Horizon::RHI {

class VulkanSemaphore : public Semaphore {
  public:
    VulkanSemaphore(const VulkanRendererContext& context) noexcept;
    virtual ~VulkanSemaphore() noexcept;

    VulkanSemaphore(const VulkanSemaphore &rhs) noexcept = delete;
    VulkanSemaphore &operator=(const VulkanSemaphore &rhs) noexcept = delete;
    VulkanSemaphore(VulkanSemaphore &&rhs) noexcept = delete;
    VulkanSemaphore &operator=(VulkanSemaphore &&rhs) noexcept = delete;

  public:
    const VulkanRendererContext& m_context;
    VkSemaphore m_semaphore;
};

} // namespace Horizon::RHI