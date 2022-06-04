#include "VulkanCommandList.h"
#include "VulkanBuffer2.h"

namespace Horizon {
	namespace RHI {

		VulkanCommandList::VulkanCommandList() noexcept
		{
		}

		VulkanCommandList::~VulkanCommandList() noexcept
		{
		}
		void VulkanCommandList::BeginRecording() noexcept
		{
			is_recording = true;
		}
		void VulkanCommandList::EndRecording() noexcept
		{
			is_recording = false;
		}
		void VulkanCommandList::Draw() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandList::DrawIndirect() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandList::Dispatch() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandList::DispatchIndirect() noexcept
		{
			//assert(is_recording, "command list isn't recording");

		}
		void VulkanCommandList::UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept
		{
			//assert(is_recording, "command list isn't recording");
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

		void VulkanCommandList::UpdateTexture() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}

		void VulkanCommandList::CopyBuffer(Buffer* dst_buffer, Buffer* src_buffer) noexcept
		{
			//assert(is_recording, "command list isn't recording");
			assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
			auto vk_src_buffer = dynamic_cast<VulkanBuffer*>(src_buffer);
			auto vk_dst_buffer = dynamic_cast<VulkanBuffer*>(dst_buffer);
			VkBufferCopy region{};
			region.size = dst_buffer->GetBufferSize();
			vkCmdCopyBuffer(m_command_buffer, vk_src_buffer->m_buffer, vk_dst_buffer->m_buffer, 1, &region);
		}
		void VulkanCommandList::CopyTexture() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandList::InsertBarrier(const BarrierDesc& desc) noexcept
		{
			//assert(is_recording, "command list isn't recording");
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

			vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 0, nullptr, desc.buffer_memory_barriers.size(), buffer_memory_barriers.data(), desc.image_memory_barriers.size(), image_memory_barriers.data());
		}
		void VulkanCommandList::Submit() noexcept
		{
			//assert(is_recording, "command list isn't recording");
		}
		void VulkanCommandList::CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept
		{
			//assert(is_recording, "command list isn't recording");
			assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
			VkBufferCopy region{};
			region.size = dst_buffer->GetBufferSize();
			vkCmdCopyBuffer(m_command_buffer, src_buffer->m_buffer, dst_buffer->m_buffer, 1, &region);
		}
	}
}

