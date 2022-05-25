#include "VulkanBuffer2.h"

namespace Horizon {
	namespace RHI {

		VulkanBuffer2::VulkanBuffer2(VmaAllocator allocator, VkBufferCreateInfo* buffer_create_info) :m_allocator(allocator)
		{
			VmaAllocationCreateInfo allocation_creat_info = {};
			allocation_creat_info.usage = VMA_MEMORY_USAGE_AUTO;
			CHECK_VK_RESULT(vmaCreateBuffer(allocator, buffer_create_info, &allocation_creat_info, &m_buffer, &m_allocation, nullptr));
		}

		VulkanBuffer2::~VulkanBuffer2()
		{
			Destroy();
		}

		void VulkanBuffer2::Destroy()
		{
			vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
		}

	}
}