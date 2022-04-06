#pragma once

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "Attachment.h"

namespace Horizon {

	class RenderPass
	{
	public:
		RenderPass(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo);
		~RenderPass();
		VkRenderPass get() const;
	private:
		void createRenderPass(const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo);
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		VkRenderPass mRenderPass;
	};
}
