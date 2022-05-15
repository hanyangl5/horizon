#include "Attachment.h"

#include <runtime/core/log/Log.h>
#include <runtime/function/render/RenderContext.h>

namespace Horizon {

	Attachment::Attachment(std::shared_ptr<Device> device, const AttachmentCreateInfo create_info)
	{
		VkImageUsageFlags usage = 0;
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		m_format = ToVkImageFormat(create_info.format);

		if (create_info.usage & AttachmentUsageFlags::COLOR_ATTACHMENT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (create_info.usage & AttachmentUsageFlags::DEPTH_STENCIL_ATTACHMENT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image_create_info{};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = ToVkImageType(create_info.texture_type);
		image_create_info.format = m_format;
		image_create_info.extent.width = create_info.width;
		image_create_info.extent.height = create_info.height;
		image_create_info.extent.depth = create_info.depth;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkMemoryAllocateInfo memAlloc{};
		VkMemoryRequirements memReqs{};

		CHECK_VK_RESULT(vkCreateImage(device->Get(), &image_create_info, nullptr, &m_image));
		vkGetImageMemoryRequirements(device->Get(), m_image, &memReqs);
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = FindMemoryType(device->getPhysicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CHECK_VK_RESULT(vkAllocateMemory(device->Get(), &memAlloc, nullptr, &m_image_memory));
		CHECK_VK_RESULT(vkBindImageMemory(device->Get(), m_image, m_image_memory, 0));

		VkImageViewCreateInfo imageView{};
		imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		switch (image_create_info.imageType)
		{
		case VK_IMAGE_TYPE_1D:
			imageView.viewType = VK_IMAGE_VIEW_TYPE_1D;
			break;
		case VK_IMAGE_TYPE_2D:
			imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VK_IMAGE_TYPE_3D:
			imageView.viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		default:
			imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			break;
		}
		imageView.format = m_format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = m_image;
		CHECK_VK_RESULT(vkCreateImageView(device->Get(), &imageView, nullptr, &m_image_view));
	}


}