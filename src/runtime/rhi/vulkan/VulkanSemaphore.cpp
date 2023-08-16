#include "VulkanSemaphore.h"

namespace Horizon::Backend {

VulkanSemaphore::VulkanSemaphore(const VulkanRendererContext &context) noexcept : m_context(context) {
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = nullptr;
    semaphore_create_info.flags = 0;
    vkCreateSemaphore(m_context.device, &semaphore_create_info, nullptr, &m_semaphore);
}

VulkanSemaphore::~VulkanSemaphore() noexcept { vkDestroySemaphore(m_context.device, m_semaphore, nullptr); }

void VulkanSemaphore::AddWaitStage([[maybe_unused]]CommandQueueType queue_type) noexcept {
    flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //switch (queue_type) {
    //case Horizon::GRAPHICS:
    //    flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //    break;
    //case Horizon::COMPUTE:
    //    flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //    break;
    //case Horizon::TRANSFER:
    //    flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //    break;
    //default:
    //    LOG_ERROR("invalid");
    //    break;
    //}
}

u32 VulkanSemaphore::GetWaitStage() noexcept {
    if (flags == 0) {
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    u32 wait_flags = flags;
    flags = 0;
    return wait_flags;
}

} // namespace Horizon::Backend