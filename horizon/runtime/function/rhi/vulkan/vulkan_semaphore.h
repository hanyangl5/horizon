#pragma once

#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/vulkan/vulkan_command_list.h>

namespace Horizon::Backend {

class VulkanSemaphore : public Semaphore {
  public:
    VulkanSemaphore(const VulkanRendererContext &context) noexcept;
    virtual ~VulkanSemaphore() noexcept;

    VulkanSemaphore(const VulkanSemaphore &rhs) noexcept = delete;
    VulkanSemaphore &operator=(const VulkanSemaphore &rhs) noexcept = delete;
    VulkanSemaphore(VulkanSemaphore &&rhs) noexcept = delete;
    VulkanSemaphore &operator=(VulkanSemaphore &&rhs) noexcept = delete;

  public:
    void AddWaitStage(CommandQueueType queue_type) noexcept override;
    u32 GetWaitStage() noexcept override;

  public:
    const VulkanRendererContext &m_context{};
    VkSemaphore m_semaphore{};
};

} // namespace Horizon::Backend