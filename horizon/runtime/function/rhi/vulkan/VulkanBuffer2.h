#pragma once

#include "vk_mem_alloc.h"

#include <runtime/function/rhi/Buffer.h>

namespace Horizon
{
	namespace RHI
	{

		class VulkanBuffer2 : public Buffer
		{
		public:
			using Buffer::Buffer;
			VulkanBuffer2(VmaAllocator allocator, const BufferCreateInfo &buffer_create_info) noexcept;
			~VulkanBuffer2() noexcept;

		private:
			virtual void Destroy() noexcept override;

		private:
			VkBuffer m_buffer;
			VmaAllocation m_allocation;
			VmaAllocator m_allocator;
		};

	}
}