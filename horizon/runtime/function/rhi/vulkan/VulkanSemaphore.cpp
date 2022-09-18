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

} // namespace Horizon::RHI