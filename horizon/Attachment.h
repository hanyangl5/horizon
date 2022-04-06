#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"

namespace Horizon {
	enum AttachmentUsageFlags {
		NONE = 0,
		COLOR_ATTACHMENT = 1,
		DEPTH_STENCIL_ATTACHMENT = 2,
		PRESENT_SRC = 4
	};
	using AttachmentUsage = u32;

	struct AttachmentCreateInfo {
	public:
		VkFormat format;
		AttachmentUsage usage = AttachmentUsageFlags::NONE;
		u32 width, height;
	};
	class Attachment{
	public:
		Attachment(std::shared_ptr<Device> device, AttachmentCreateInfo createInfo);

		VkImage mImage;
		VkDeviceMemory mImageMemory;
		VkImageView mImageView;
		VkFormat mFormat;
	};

	class AttachmentDescriptor :public DescriptorBase {
		
	};
}