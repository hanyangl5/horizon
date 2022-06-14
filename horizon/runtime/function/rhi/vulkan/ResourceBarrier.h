#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include <runtime/function/rhi/vulkan/CommandBuffer.h>
#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/rhi/vulkan/Texture.h>

namespace Horizon{

	struct BufferMemoryBarrierDesc {
		void* buffer;
		u32 offset;
		u64 size;
		u32 src_access_mask, dst_access_mask;
		u32 src_queue_family_index = VK_QUEUE_FAMILY_IGNORED, dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
	};

	struct ImageMemoryBarrierDesc {
		MemoryAccessFlags src_access_mask, dst_access_mask;
		TextureUsage src_usage, dst_usage; // transition image layout
		std::shared_ptr<Texture> texture;
		u32 src_queue_family_index = VK_QUEUE_FAMILY_IGNORED, dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
	};

	struct BarrierDesc {
		u32 src_stage, dst_stage;
		std::vector<BufferMemoryBarrierDesc> buffer_memory_barriers;
		std::vector<ImageMemoryBarrierDesc> image_memory_barriers;
	};

	void InsertBarrier(u32 i, std::shared_ptr<CommandBuffer> command_buffer, const BarrierDesc& desc) noexcept;
	
}