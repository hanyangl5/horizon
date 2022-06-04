#pragma once

#include "vk_mem_alloc.h"

#include <runtime/function/rhi/Buffer.h>

namespace Horizon
{
	namespace RHI
	{

		class VulkanBuffer : public Buffer
		{
		public:
			using Buffer::Buffer;
			VulkanBuffer(VmaAllocator allocator, const BufferCreateInfo &buffer_create_info, MemoryFlag memory_flag) noexcept;
			~VulkanBuffer() noexcept;
		private:
			virtual void Destroy() noexcept override;

		public:
			VkBuffer m_buffer;
			VmaAllocation m_memory;
			VmaAllocator m_allocator;
		};

	}
}