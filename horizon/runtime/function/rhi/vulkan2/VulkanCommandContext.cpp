#include "VulkanCommandContext.h"
#include "VulkanBuffer2.h"

namespace Horizon {
	namespace RHI {

		VulkanCommandContext::VulkanCommandContext(VkDevice device) noexcept : m_device(device)
		{
			// each thread has 3 types of command pool, can allocate 3 types of command buffer
			for (u32 i = 0; i < m_command_pools.size(); i++) {
				VkCommandPoolCreateInfo command_pool_create_info{};
				command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				command_pool_create_info.queueFamilyIndex = i;
				command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

				CHECK_VK_RESULT(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &m_command_pools[i]));
			}
		}

		VulkanCommandContext::~VulkanCommandContext() noexcept
		{
		}
		void VulkanCommandContext::BeginRecording() noexcept
		{
			is_recording = true;

		}
		void VulkanCommandContext::EndRecording() noexcept
		{
			is_recording = false;
		}
		void VulkanCommandContext::Draw() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandContext::DrawIndirect() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandContext::Dispatch() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandContext::DispatchIndirect() noexcept
		{
			//assert(is_recording, "command list isn't recording");

		}
		void VulkanCommandContext::UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept
		{
			
		}

		void VulkanCommandContext::UpdateTexture() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}

		void VulkanCommandContext::CopyBuffer(Buffer* dst_buffer, Buffer* src_buffer) noexcept
		{
			//assert(is_recording, "command list isn't recording");
			assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
			auto vk_src_buffer = dynamic_cast<VulkanBuffer*>(src_buffer);
			auto vk_dst_buffer = dynamic_cast<VulkanBuffer*>(dst_buffer);
			VkBufferCopy region{};
			region.size = dst_buffer->GetBufferSize();
			//vkCmdCopyBuffer(m_command_buffer, vk_src_buffer->m_buffer, vk_dst_buffer->m_buffer, 1, &region);
		}
		void VulkanCommandContext::CopyTexture() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandContext::InsertBarrier(const BarrierDesc& desc) noexcept
		{
		
		}
		void VulkanCommandContext::Submit() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandContext::RequestSecondaryCommandBuffer() noexcept
		{
			//VkCommandBuffer cmd;

			//VkCommandBufferAllocateInfo command_buffer_allocate_info{};
			//command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			//command_buffer_allocate_info.commandPool = m_command_pool;
			//command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			//command_buffer_allocate_info.commandBufferCount = 1;
			//CHECK_VK_RESULT(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &cmd));
			////m_second_command_buffer.emplace_back(cmd);
		}
		void VulkanCommandContext::BeginRenderPass() noexcept
		{

		}
		VkCommandBuffer VulkanCommandContext::GetCommandBuffer(CommandQueueType type) noexcept
		{
			if (m_command_buffers[type] == VK_NULL_HANDLE) {
				VkCommandBufferAllocateInfo command_buffer_allocate_info{};
				command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				command_buffer_allocate_info.commandPool = m_command_pools[type];
				command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				command_buffer_allocate_info.commandBufferCount = 1;
				CHECK_VK_RESULT(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &m_command_buffers[type]));
			}
			return m_command_buffers[type];
		}
		VkCommandBuffer VulkanCommandContext::GetSecondaryCommandBuffer() noexcept
		{
			if (m_secondary_command_buffer == VK_NULL_HANDLE) {
				VkCommandBufferAllocateInfo command_buffer_allocate_info{};
				command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				command_buffer_allocate_info.commandPool = m_command_pools[CommandQueueType::GRAPHICS];
				command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
				command_buffer_allocate_info.commandBufferCount = 1;
				CHECK_VK_RESULT(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &m_secondary_command_buffer));
			}
			return m_secondary_command_buffer;
		}
		void VulkanCommandContext::CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept
		{

		}
	}
}

