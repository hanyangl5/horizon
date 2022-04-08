#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "Device.h"
#include "SwapChain.h"
#include "RenderPass.h"
namespace Horizon {
	class Framebuffer
	{
	public:
		Framebuffer(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext, std::shared_ptr<SwapChain> swapChain = nullptr);
		~Framebuffer();
		VkFramebuffer get() const;
		VkFramebuffer get(u32 index) const;
		VkRenderPass getRenderPass() const;
		std::shared_ptr<AttachmentDescriptor> getDescriptorImageInfo(u32 attachmentIndex);
		std::vector<VkImage> getPresentImages();
		u32 getColorAttachmentCount();
		std::vector<VkClearValue> getClearValues();
	private:
		void createFrameBuffer(u32 width, u32 height, u32 imageCount, std::shared_ptr<SwapChain> swapChain = nullptr);
		void createAttachmentsResources(const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo);
	private:
		RenderContext& mRenderContext;
		bool hasDepthAttachment = false;
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<RenderPass> mRenderPass;
		std::vector<VkFramebuffer> mFramebuffer;
		// Shared sampler used for all color attachments
		VkSampler mSampler;
		std::vector<Attachment> mFramebufferAttachments;

	};
}
