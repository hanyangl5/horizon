#include "VulkanSemaphore.h"

namespace Horizon::RHI {

VulkanSemaphore::VulkanSemaphore(const VulkanRendererContext &context) noexcept : m_context(context) {
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = nullptr;
    semaphore_create_info.flags = 0;
    vkCreateSemaphore(m_context.device, &semaphore_create_info, nullptr, &m_semaphore);
}

VulkanSemaphore::~VulkanSemaphore() noexcept { vkDestroySemaphore(m_context.device, m_semaphore, nullptr); }

void VulkanSemaphore::AddWaitStage(CommandQueueType queue_type) noexcept {
    switch (queue_type) {
    case Horizon::GRAPHICS:
        flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        break;
    case Horizon::COMPUTE:
        flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        break;
    case Horizon::TRANSFER:
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    default:
        LOG_ERROR("invalid");
        break;
    }
}

u32 VulkanSemaphore::GetWaitStage() noexcept {
    if (flags == 0) {
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    u32 wait_flags = flags;
    flags = 0;
    return wait_flags;
}

} // namespace Horizon::RHI