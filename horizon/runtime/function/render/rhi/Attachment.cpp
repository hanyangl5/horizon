#include "Attachment.h"

namespace Horizon {
	Attachment::Attachment(std::shared_ptr<Device> device, AttachmentCreateInfo createInfo)
	{
		VkImageUsageFlags usage = 0;
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		m_format = createInfo.format;

		if (createInfo.usage & AttachmentUsageFlags::COLOR_ATTACHMENT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (createInfo.usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image_create_info{};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = createInfo.format;
		image_create_info.extent.width = createInfo.width;
		image_create_info.extent.height = createInfo.height;
		image_create_info.extent.depth = 1;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkMemoryAllocateInfo memAlloc{};
		VkMemoryRequirements memReqs{};

		printVkError(vkCreateImage(device->get(), &image_create_info, nullptr, &m_image));
		vkGetImageMemoryRequirements(device->get(), m_image, &memReqs);
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = findMemoryType(device->getPhysicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		printVkError(vkAllocateMemory(device->get(), &memAlloc, nullptr, &m_image_memory));
		printVkError(vkBindImageMemory(device->get(), m_image, m_image_memory, 0));

		VkImageViewCreateInfo imageView{};
		imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.format = createInfo.format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = m_image;
		printVkError(vkCreateImageView(device->get(), &imageView, nullptr, &m_image_view));
	}


}