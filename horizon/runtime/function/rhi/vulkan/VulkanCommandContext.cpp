#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanCommandContext.h>

namespace Horizon::RHI {

VulkanCommandContext::VulkanCommandContext(VkDevice device) noexcept
    : m_device(device) {
    m_command_lists_count.fill(0);
}

VulkanCommandContext::~VulkanCommandContext() noexcept {
    // destroy command pools and release all command buffers allocated from
    // pools
    for (auto &pool : m_command_pools) {
        vkDestroyCommandPool(m_device, pool, nullptr);
    }
    for (auto &cls : m_command_lists) {
        for (auto &cl : cls) {
            cl = nullptr; // release VulkanCommandLists
        }
    }
}

VulkanCommandList *
VulkanCommandContext::GetVulkanCommandList(CommandQueueType type) noexcept {
    // lazy create command pool
    if (m_command_pools[type] == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo command_pool_create_info{};
        command_pool_create_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.queueFamilyIndex = type;
        command_pool_create_info.flags =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        CHECK_VK_RESULT(vkCreateCommandPool(m_device, &command_pool_create_info,
                                            nullptr, &m_command_pools[type]));
    }

    u32 count{m_command_lists_count[type]};

    if (count >= m_command_lists[type].size()) {
        VkCommandBuffer command_buffer;
        VkCommandBufferAllocateInfo command_buffer_allocate_info{};
        command_buffer_allocate_info.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = m_command_pools[type];
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = 1;
        CHECK_VK_RESULT(vkAllocateCommandBuffers(
            m_device, &command_buffer_allocate_info, &command_buffer));
        m_command_lists[type].emplace_back(
            std::make_unique<VulkanCommandList>(type, command_buffer));
    }

    m_command_lists_count[type]++;
    return m_command_lists[type][count].get();
}

void VulkanCommandContext::Reset() noexcept {
    // reset command buffers to initial state

    for (auto &command_pool : m_command_pools) {
        if (command_pool) {
            vkResetCommandPool(m_device, command_pool,
                               VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        }
    }

    m_command_lists_count.fill(0);
}

} // namespace Horizon::RHI
