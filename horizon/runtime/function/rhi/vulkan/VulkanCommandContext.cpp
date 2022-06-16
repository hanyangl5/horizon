#include "VulkanCommandContext.h"
#include "VulkanBuffer.h"

namespace Horizon {
	namespace RHI {

		VulkanCommandContext::VulkanCommandContext(VkDevice device) noexcept : m_device(device)
		{
			m_command_lists_count.fill(0);
		}

		VulkanCommandContext::~VulkanCommandContext() noexcept
		{
			Reset();
		}

		VulkanCommandList* VulkanCommandContext::GetVulkanCommandList(CommandQueueType type) noexcept
		{
			// lazy create command pool
			if (m_command_pools[type] == VK_NULL_HANDLE) {
				VkCommandPoolCreateInfo command_pool_create_info{};
				command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				command_pool_create_info.queueFamilyIndex = type;
				command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

				CHECK_VK_RESULT(vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &m_command_pools[type]));
			}

			u32 count = m_command_lists_count[type];

			if (count >= m_command_lists[type].size()) {
				VkCommandBuffer command_buffer;
				VkCommandBufferAllocateInfo command_buffer_allocate_info{};
				command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				command_buffer_allocate_info.commandPool = m_command_pools[type];
				command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				command_buffer_allocate_info.commandBufferCount = 1;
				CHECK_VK_RESULT(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &command_buffer));
				m_command_lists[type].emplace_back(new VulkanCommandList(type, command_buffer));
			}

			m_command_lists_count[type]++;
			return m_command_lists[type][count];
		}

		void VulkanCommandContext::Reset() noexcept
		{
			// reset command buffers to initial state

			for (auto& command_pool : m_command_pools) {
				if (command_pool) {
					vkResetCommandPool(m_device, command_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
				}
			}

			// reset command buffers to reuse
			//for (u32 type = 0; type < 3;type++) {
			//	for (auto& cmdlist : m_command_lists[type]) {
			//		if (cmdlist) {
			//			//vkFreeCommandBuffers(m_device, m_command_pools[type], 1, &cmdlist->m_command_buffer);
			//			//vkResetCommandBuffer(cmdlist->m_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
			//		}
			//	}
			//}

			m_command_lists_count.fill(0);

		}
	}
}

