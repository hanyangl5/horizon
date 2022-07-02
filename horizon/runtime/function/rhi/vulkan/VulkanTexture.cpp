#include <runtime/function/rhi/vulkan/VulkanTexture.h>

namespace Horizon::RHI {

	VulkanTexture::VulkanTexture(VmaAllocator allocator, const TextureCreateInfo& texture_create_info) noexcept : Texture(texture_create_info), m_allocator(allocator)
	{
		VkImageCreateInfo image_create_info{};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = ToVkImageType(texture_create_info.texture_type);
		image_create_info.extent.width = texture_create_info.width;
		image_create_info.extent.height = texture_create_info.height;
		image_create_info.extent.depth = texture_create_info.depth;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.format = ToVkImageFormat(texture_create_info.texture_format);
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_create_info.usage = ToVkImageUsage(texture_create_info.texture_usage);
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;

		VmaAllocationCreateInfo allocation_creat_info = {};
		allocation_creat_info.usage = VMA_MEMORY_USAGE_AUTO;

		CHECK_VK_RESULT(vmaCreateImage(allocator, &image_create_info, &allocation_creat_info, &m_image, &m_allocation, nullptr));
	}

	VulkanTexture::~VulkanTexture() noexcept
	{
		Destroy();
	}

	void VulkanTexture::Destroy() noexcept
	{
		vmaDestroyImage(m_allocator, m_image, m_allocation);
	}

}
