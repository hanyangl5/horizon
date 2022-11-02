#pragma once

#include <vulkan/vulkan.h>



#include <runtime/function/rhi/CommandContext.h>
#include <runtime/function/rhi/vulkan/VulkanCommandList.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::Backend {

class VulkanCommandContext : public CommandContext {
  public:
    VulkanCommandContext(const VulkanRendererContext &context) noexcept;

    virtual ~VulkanCommandContext() noexcept override;

    VulkanCommandContext(const VulkanCommandContext &command_list) noexcept = delete;
    VulkanCommandContext(VulkanCommandContext &&command_list) noexcept = delete;

    VulkanCommandContext &operator=(const VulkanCommandContext &rhs) noexcept = delete;

    VulkanCommandContext &operator=(VulkanCommandContext &&rhs) noexcept = delete;

    virtual CommandList *GetCommandList(CommandQueueType type) override;
    virtual void Reset() override;

  private:
    const VulkanRendererContext &m_context{};
    // each thread has pools to allocate graphics/compute/transfer commandlist
    Container::FixedArray<VkCommandPool, 3> m_command_pools{};

    Container::FixedArray<Container::Array<VulkanCommandList* >, 3> m_command_lists{};
    Container::FixedArray<u32, 3> m_command_lists_count{};
};
} // namespace Horizon::Backend
