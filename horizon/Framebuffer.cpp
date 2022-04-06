#include "Framebuffer.h"

namespace Horizon {

	Framebuffer::Framebuffer(std::shared_ptr<Device> device, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext, std::shared_ptr<SwapChain> swapChain) :mRenderContext(renderContext), mDevice(device)
	{
		createAttachmentsResources(attachmentsCreateInfo);
		mRenderPass = std::make_shared<RenderPass>(mDevice, attachmentsCreateInfo);
		if (swapChain) {
			createFrameBuffer(mRenderContext.width, mRenderContext.height, mRenderContext.swapChainImageCount, swapChain);
		}
		else {
			createFrameBuffer(mRenderContext.width, mRenderContext.height, 1);
		}
	}

	Framebuffer::~Framebuffer()
	{
		for (auto& attachment : mFramebufferAttachments) {
			vkDestroyImage(mDevice->get(), attachment.mImage, nullptr);
			vkDestroyImageView(mDevice->get(), attachment.mImageView, nullptr);
			vkFreeMemory(mDevice->get(), attachment.mImageMemory, nullptr);
		}
		for (auto& framebuffer : mFramebuffer) {
			vkDestroyFramebuffer(mDevice->get(), framebuffer, nullptr);
		}
	}

	VkFramebuffer Framebuffer::get() const
	{
		return mFramebuffer[0];
	}

	VkFramebuffer Framebuffer::get(u32 index) const
	{
		return mFramebuffer[index];
	}

	VkRenderPass Framebuffer::getRenderPass() const
	{
		return mRenderPass->get();
	}

	std::shared_ptr<AttachmentDescriptor> Framebuffer::getDescriptorImageInfo(u32 attachmentIndex)
	{
		std::shared_ptr<AttachmentDescriptor> attachmentDescriptor = std::make_shared<AttachmentDescriptor>();
		attachmentDescriptor->imageDescriptorInfo = { mSampler, mFramebufferAttachments[attachmentIndex].mImageView ,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		return attachmentDescriptor;
	}

	std::vector<VkImage> Framebuffer::getPresentImages()
	{
		//std::vector<VkImage> images;
		//for (auto& framebuffer : mFramebuffer) {
		//	framebuffer
		//}
		//for (auto& attachment : mFramebufferAttachments) {
		//	images.push_back(attachment.mImage);
		//}
		//return ();
		return {};
	}

	void Framebuffer::createFrameBuffer(u32 width, u32 height, u32 imageCount, std::shared_ptr<SwapChain> swapChain)
	{
		mFramebuffer.resize(imageCount);
		for (u32 i = 0; i < imageCount; i++)
		{
			std::vector<VkImageView> attachmentsImageViews{};
			if (swapChain) {
				attachmentsImageViews.emplace_back(swapChain->getImageView(i));
			}
			else {
				attachmentsImageViews.resize(mFramebufferAttachments.size());
				for (u32 j = 0; j < mFramebufferAttachments.size(); j++) {
					attachmentsImageViews[j] = mFramebufferAttachments[j].mImageView;
				}
			}
			VkFramebufferCreateInfo frameBufferCreateInfo{};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.renderPass = mRenderPass->get();
			frameBufferCreateInfo.attachmentCount = static_cast<u32>(attachmentsImageViews.size());
			frameBufferCreateInfo.pAttachments = attachmentsImageViews.data();
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.layers = 1;
			printVkError(vkCreateFramebuffer(mDevice->get(), &frameBufferCreateInfo, nullptr, &mFramebuffer[i]), "create frame buffer");
		}
	}

	void Framebuffer::createAttachmentsResources(const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo)
	{
		for (auto& createInfo : attachmentsCreateInfo) {
			mFramebufferAttachments.emplace_back(mDevice, createInfo);
		}

		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		printVkError(vkCreateSampler(mDevice->get(), &sampler, nullptr, &mSampler));

	}




}