#include "VulkanCommandList.h"
#include "VulkanBuffer2.h"

namespace Horizon {
	namespace RHI {

		VulkanCommandContext::VulkanCommandContext(VkDevice device) noexcept : m_device(device)
		{
		}

		VulkanCommandContext::~VulkanCommandContext() noexcept
		{
		}

		VulkanCommandList VulkanCommandContext::GetVulkanCommandList(CommandQueueType type) noexcept
		{
			// lazy create command pool
			if (m_command_pools[type] == VK_NULL_HANDLE) {
				VkCommandPoolCreateInfo command_pool_create_info{};
				command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				command_pool_create_info.queueFamilyIndex = type;
				command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

				CHECK_VK_RESULT(vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &m_command_pools[type]));
			}

			// allocate command buffer
			VkCommandBuffer command_buffer;
			VkCommandBufferAllocateInfo command_buffer_allocate_info{};
			command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_allocate_info.commandPool = m_command_pools[type];
			command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_allocate_info.commandBufferCount = 1;
			CHECK_VK_RESULT(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &command_buffer));

			return VulkanCommandList(command_buffer, type);
		}
		
		void Get() const noexcept{
			return m_command_buffer;	
		}

		void BeginRecoridng() noexcept{
			VkCommandBufferBeginInfo command_buffer_begin_info{};
			command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
		}

		void EndRecording() noexcept{
			VkEndCommandBuffer(command_buffer);
		}

		// graphics commands
		void BeginRenderPass() noexcept{
			if(m_type != CommandQueueType::GRAPHICS){
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
			VkRenderPassBeginInfo render_pass_begin_info;

			vkCmdBeginRenderPass(m_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}

		void EndRenderPass() noexcept{
			if(m_type != CommandQueueType::GRAPHICS){
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
			// for (auto& cmdbuf : m_command_context_map) {
			// 	//m_vulkan.secondary_command_buffer.emplace_back(cmdbuf.second->);
			// }

			// vkCmdExecuteCommands(m_vulkan.primary_command_buffer, m_vulkan.secondary_command_buffer.size(), m_vulkan.secondary_command_buffer.data());

			vkCmdEndRenderPass(m_command_buffer);
		}
		void Draw() noexcept{
			if(m_type != CommandQueueType::GRAPHICS){
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
		}
		void DrawIndirect() noexcept{
			if(m_type != CommandQueueType::GRAPHICS){
				LOG_ERROR("invalid commands for current commandlist, expect graphics commandlist");
				return;
			}
		}	

		// compute commands
		void Dispatch() noexcept{
			if(m_type != CommandQueueType::COMPUTE){
				LOG_ERROR("invalid commands for current commandlist, expect compute commandlist");
				return;
			}
		}
		void DispatchIndirect() noexcept{
			if(m_type != CommandQueueType::COMPUTE){
				LOG_ERROR("invalid commands for current commandlist, expect compute commandlist");
				return;
			}		
		}

		// transfer commands
		void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept{
			if(m_type != CommandQueueType::COMPUTE){
				LOG_ERROR("invalid commands for current commandlist, expect compute commandlist");
				return;
			}
		}

		void CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept{
			if(m_type != CommandQueueType::TRANSFER){
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
				return;
			}
			auto vk_src_buffer = dynamic_cast<VulkanBuffer*>(src_buffer);
			auto vk_dst_buffer = dynamic_cast<VulkanBuffer*>(dst_buffer);
			CopyBuffer(vk_src_buffer, vk_dst_buffer);
		}

		void CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept{
			assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
			VkBufferCopy region{};
			region.size = dst_buffer->GetBufferSize();
			vkCmdCopyBuffer(m_command_buffer), src_buffer->m_buffer, dst_buffer->m_buffer, 1, &region);
		}
		void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept{
			if(m_type != CommandQueueType::TRANSFER){
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
				return;
			}
	
			assert(buffer->GetBufferSize() == size);

			auto vk_buffer = dynamic_cast<VulkanBuffer*>(buffer);
			auto stage_buffer = new VulkanBuffer(vk_buffer->m_allocator, BufferCreateInfo{ BufferUsage::BUFFER_USAGE_TRANSFER_SRC, size }, MemoryFlag::CPU_VISABLE_MEMORY);

			// persistantly mapping 
			memcpy(stage_buffer->m_memory, &data, size);

			//barrier 1
			//{
			//	BufferMemoryBarrierDesc bmb;
			//	bmb.src_access_mask = MemoryAccessFlags::ACCESS_HOST_WRITE_BIT;
			//	bmb.src_access_mask = MemoryAccessFlags::ACCESS_TRANSFER_READ_BIT;

			//	BarrierDesc desc{};
			//	desc.src_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
			//	desc.dst_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
			//	InsertBarrier(desc);
			//}

			// upload to gpu buffer

			CopyBuffer(vk_buffer, stage_buffer);

			// barrier2
			//{
			//	BarrierDesc desc{};
			//	desc.src_stage = PipelineStageFlags::PIPELINE_STAGE_TRANSFER_BIT;
			//	desc.dst_stage = UsageToPipelineStage(vk_buffer->GetBufferUsage());
			//	InsertBarrier(desc);
			//}


			delete stage_buffer;
		}

		void UpdateTexture() noexcept{
			if(m_type != CommandQueueType::TRANSFER){
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
			}
		}

		void CopyTexture() noexcept{
			if(m_type != CommandQueueType::TRANSFER){
				LOG_ERROR("invalid commands for current commandlist, expect transfer commandlist");
			}
		}

		void InsertBarrier(const BarrierDesc& desc) noexcept{

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

			for (u32 i = 0; i < desc.image_memory_barriers.size(); i++) {
				image_memory_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				image_memory_barriers[i].srcAccessMask = ToVkMemoryAccessFlags(desc.image_memory_barriers[i].src_access_mask);
				image_memory_barriers[i].dstAccessMask = ToVkMemoryAccessFlags(desc.image_memory_barriers[i].dst_access_mask);
				image_memory_barriers[i].oldLayout = ToVkImageLayout(desc.image_memory_barriers[i].src_usage);
				image_memory_barriers[i].newLayout = ToVkImageLayout(desc.image_memory_barriers[i].dst_usage);
				image_memory_barriers[i].image = desc.image_memory_barriers[i].texture->GetImage();
				image_memory_barriers[i].subresourceRange = desc.image_memory_barriers[i].texture->GetSubresourceRange();

			}

			vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 0, nullptr, 
				desc.buffer_memory_barriers.size(), buffer_memory_barriers.data(), desc.image_memory_barriers.size(), image_memory_barriers.data());
		}

	}
}

