#pragma once

#include <runtime/function/rhi/RHIUtils.h>

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
		void* texture;
		u32 src_queue_family_index = VK_QUEUE_FAMILY_IGNORED, dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
	};

	struct BarrierDesc {
		u32 src_stage, dst_stage;
		std::vector<BufferMemoryBarrierDesc> buffer_memory_barriers;
		std::vector<ImageMemoryBarrierDesc> image_memory_barriers;
	};

	
}