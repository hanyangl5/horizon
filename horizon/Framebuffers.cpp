#include "Framebuffers.h"

namespace Horizon {

	Framebuffers::Framebuffers(std::shared_ptr<Device> device, std::shared_ptr<SwapChain> swapchain, std::shared_ptr<RenderPass> renderPass) :mDevice(device), mSwapChain(swapchain), mRenderPass(renderPass)
	{
		createFrameBuffers();
	}

	Framebuffers::~Framebuffers()
	{
		for (auto framebuffer : mSwapChainFrameBuffers)
		{
			vkDestroyFramebuffer(mDevice->get(), framebuffer, nullptr);
		}
	}

	VkFramebuffer Framebuffers::get(u32 index) const
	{
		return mSwapChainFrameBuffers[index];
	}

	void Framebuffers::createFrameBuffers()
	{
		mSwapChainFrameBuffers.resize(mSwapChain->getImageCount());

		for (u32 i = 0; i < mSwapChain->getImageCount(); i++)
		{
			std::vector <VkImageView> attachments = {
				mSwapChain->getImageView(i),
				mSwapChain->getDepthImageView()
			};

			VkFramebufferCreateInfo frameBufferCreateInfo{};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.renderPass = mRenderPass->get();
			frameBufferCreateInfo.attachmentCount = static_cast<u32>(attachments.size());
			frameBufferCreateInfo.pAttachments = attachments.data();
			frameBufferCreateInfo.width = mSwapChain->getExtent().width;
			frameBufferCreateInfo.height = mSwapChain->getExtent().height;
			frameBufferCreateInfo.layers = 1;
			printVkError(vkCreateFramebuffer(mDevice->get(), &frameBufferCreateInfo, nullptr, &mSwapChainFrameBuffers[i]), "create frame buffer");
		}
	}


}