#include "VulkanBuffer2.h"

namespace Horizon
{
	namespace RHI
	{

		VulkanBuffer2::VulkanBuffer2(VmaAllocator allocator, const BufferCreateInfo &buffer_create_info) noexcept : Buffer(buffer_create_info), m_allocator(allocator)
		{
			VkBufferCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			create_info.size = buffer_create_info.size;
			create_info.usage = ToVulkanBufferUsage(buffer_create_info.buffer_usage_flags);

			VmaAllocationCreateInfo allocation_creat_info = {};
			allocation_creat_info.usage = VMA_MEMORY_USAGE_AUTO;
			CHECK_VK_RESULT(vmaCreateBuffer(allocator, &create_info, &allocation_creat_info, &m_buffer, &m_allocation, nullptr));
		}

		VulkanBuffer2::~VulkanBuffer2() noexcept
		{
			Destroy();
		}

		void VulkanBuffer2::Destroy() noexcept
		{
			vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
		}

	}
}