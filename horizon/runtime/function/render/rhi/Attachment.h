#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.hpp>

#include <runtime/core/math/Math.h>
#include <runtime/function/render/rhi/VulkanEnums.h>
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

		VkImage m_image;
		VkDeviceMemory m_image_memory;
		VkImageView m_image_view;
		VkFormat m_format;
	};

	class AttachmentDescriptor :public DescriptorBase {
		
	};
}