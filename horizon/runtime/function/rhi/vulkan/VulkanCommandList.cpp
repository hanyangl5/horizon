#include "VulkanCommandContext.h"
#include "VulkanBuffer.h"

namespace Horizon {
	namespace RHI {

		VulkanCommandList::VulkanCommandList(CommandQueueType type, VkCommandBuffer command_buffer, const VulkanCommandContext* command_context) noexcept : CommandList(type), m_command_buffer(command_buffer), m_command_context(command_context)
		{

		}

		VulkanCommandList::~VulkanCommandList() noexcept
		{
		}

		void VulkanCommandList::BeginRecording() noexcept {
			is_recoring = true;
			VkCommandBufferBeginInfo command_buffer_begin_info{};
			command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_command_buffer, &command_buffer_begin_info);
		}

		void VulkanCommandList::EndRecording() noexcept {
			is_recoring = false;
			vkEndCommandBuffer(m_command_buffer);
		}

		// graphics commands
		void VulkanCommandList::BeginRenderPass() noexcept {

			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}

			if (m_type != CommandQueueType::GRAPHICS) {
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
			VkRenderPassBeginInfo render_pass_begin_info;

			vkCmdBeginRenderPass(m_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}

		void VulkanCommandList::EndRenderPass() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::GRAPHICS) {
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
			// for (auto& cmdbuf : m_command_context_map) {
			// 	//m_vulkan.secondary_command_buffer.emplace_back(cmdbuf.second->);
			// }

			// vkCmdExecuteCommands(m_vulkan.primary_command_buffer, m_vulkan.secondary_command_buffer.size(), m_vulkan.secondary_command_buffer.data());

			vkCmdEndRenderPass(m_command_buffer);
		}
		void VulkanCommandList::Draw() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::GRAPHICS) {
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
		}
		void VulkanCommandList::DrawIndirect() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::GRAPHICS) {
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
		}

		// compute commands
		void VulkanCommandList::Dispatch() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::COMPUTE) {
				LOG_ERROR("invalid commands for current commandlist, expect compute commandlist");
				return;
			}
		}
		void VulkanCommandList::DispatchIndirect() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::COMPUTE) {
				LOG_ERROR("invalid commands for current commandlist, expect compute commandlist");
				return;
			}
		}

		// transfer commands
		void VulkanCommandList::UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::TRANSFER) {
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
				return;
			}
			// if prev buffer and current are same not upload
			{

			}
			assert(buffer->GetBufferSize() == size);

			auto vk_buffer = dynamic_cast<VulkanBuffer*>(buffer);
			//auto stage_buffer = new VulkanBuffer(vk_buffer->m_allocator, BufferCreateInfo{ BufferUsage::BUFFER_USAGE_TRANSFER_SRC, size }, MemoryFlag::CPU_VISABLE_MEMORY);
			//auto stage_buffer = stage_pool->GetStageBuffer();
			VulkanBuffer* stage_buffer = m_command_context->GetStageBuffer(BufferCreateInfo{ BufferUsage::BUFFER_USAGE_TRANSFER_SRC, size });
			void* mapped_data;
			vmaMapMemory(stage_buffer->m_allocator, stage_buffer->m_memory, &mapped_data);
			memcpy(mapped_data, &data, size);
			vmaUnmapMemory(stage_buffer->m_allocator, stage_buffer->m_memory);

			//barrier 1
			{
				BufferMemoryBarrierDesc bmb{};
				bmb.src_access_mask = MemoryAccessFlags::ACCESS_HOST_WRITE_BIT;
				bmb.dst_access_mask = MemoryAccessFlags::ACCESS_TRANSFER_READ_BIT;
				bmb.buffer = stage_buffer->GetBufferPointer();
				bmb.offset = 0;
				bmb.size = stage_buffer->GetBufferSize();

				BarrierDesc desc{};
				desc.src_stage = PipelineStageFlags::PIPELINE_STAGE_HOST_BIT;
				desc.dst_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
				desc.buffer_memory_barriers.emplace_back(bmb);

				InsertBarrier(desc);
			}

			// upload to gpu buffer

			CopyBuffer(stage_buffer, vk_buffer);

			//delete stage_buffer;
			//stage_buffer = nullptr;
		}

		void VulkanCommandList::CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::TRANSFER) {
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
				return;
			}
			auto vk_src_buffer = dynamic_cast<VulkanBuffer*>(src_buffer);
			auto vk_dst_buffer = dynamic_cast<VulkanBuffer*>(dst_buffer);
			CopyBuffer(vk_src_buffer, vk_dst_buffer);
		}

		void VulkanCommandList::CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept {
			assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
			VkBufferCopy region{};
			region.size = dst_buffer->GetBufferSize();
			vkCmdCopyBuffer(m_command_buffer, src_buffer->m_buffer, dst_buffer->m_buffer, 1, &region);
		}


		void VulkanCommandList::UpdateTexture() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::TRANSFER) {
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
			}
		}

		void VulkanCommandList::CopyTexture() noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}
			if (m_type != CommandQueueType::TRANSFER) {
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
			}
		}

		void VulkanCommandList::InsertBarrier(const BarrierDesc& desc) noexcept {
			if (!is_recoring) {
				LOG_ERROR("command buffer isn't recording");
				return;
			}

			VkPipelineStageFlags src_stage = ToVkPipelineStage(desc.src_stage);
			VkPipelineStageFlags dst_stage = ToVkPipelineStage(desc.dst_stage);

			std::vector<VkBufferMemoryBarrier> buffer_memory_barriers(desc.buffer_memory_barriers.size());
			std::vector<VkImageMemoryBarrier> image_memory_barriers(desc.image_memory_barriers.size());

			for (u32 i = 0; i < desc.buffer_memory_barriers.size(); i++) {
				buffer_memory_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				buffer_memory_barriers[i].srcAccessMask = ToVkMemoryAccessFlags(desc.buffer_memory_barriers[i].src_access_mask);
				buffer_memory_barriers[i].dstAccessMask = ToVkMemoryAccessFlags(desc.buffer_memory_barriers[i].dst_access_mask);
				buffer_memory_barriers[i].buffer = static_cast<VkBuffer>(desc.buffer_memory_barriers[i].buffer);
				buffer_memory_barriers[i].offset = desc.buffer_memory_barriers[i].offset;
				buffer_memory_barriers[i].size = desc.buffer_memory_barriers[i].size;
			}

			//for (u32 i = 0; i < desc.image_memory_barriers.size(); i++) {
			//	image_memory_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			//	image_memory_barriers[i].srcAccessMask = ToVkMemoryAccessFlags(desc.image_memory_barriers[i].src_access_mask);
			//	image_memory_barriers[i].dstAccessMask = ToVkMemoryAccessFlags(desc.image_memory_barriers[i].dst_access_mask);
			//	image_memory_barriers[i].oldLayout = ToVkImageLayout(desc.image_memory_barriers[i].src_usage);
			//	image_memory_barriers[i].newLayout = ToVkImageLayout(desc.image_memory_barriers[i].dst_usage);
			//	image_memory_barriers[i].image = desc.image_memory_barriers[i].texture->GetImage();
			//	image_memory_barriers[i].subresourceRange = desc.image_memory_barriers[i].texture->GetSubresourceRange();

			//}

			vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 0, nullptr,
				desc.buffer_memory_barriers.size(), buffer_memory_barriers.data(), desc.image_memory_barriers.size(), image_memory_barriers.data());
		}

	}
}

