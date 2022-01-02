#include "Framebuffers.h"

Framebuffers::Framebuffers(std::shared_ptr<Device> device,std::shared_ptr<SwapChain> swapchain,std::shared_ptr<Pipeline> pipeline):mDevice(device),mSwapChain(swapchain),mPipeline(pipeline)
{
    mSwapChainFrameBuffers.resize(mSwapChain->getImageCount());

    for (u32 i = 0; i < mSwapChain->getImageCount(); i++)
    {
        VkImageView attachments[] = {
            mSwapChain->getImageView(i)
        };

        VkFramebufferCreateInfo frameBufferCreateInfo{};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = mPipeline->getRenderPass();
        frameBufferCreateInfo.attachmentCount = 1;
        frameBufferCreateInfo.pAttachments = attachments;
        frameBufferCreateInfo.width = mSwapChain->getExtent().width;
        frameBufferCreateInfo.height = mSwapChain->getExtent().height;
        frameBufferCreateInfo.layers = 1;
        printVkError(vkCreateFramebuffer(mDevice->get(), &frameBufferCreateInfo, nullptr, &mSwapChainFrameBuffers[i]), "create frame buffer");
    }
}


Framebuffers::~Framebuffers()
{
    for (auto framebuffer : mSwapChainFrameBuffers)
    {
        vkDestroyFramebuffer(mDevice->get(), framebuffer, nullptr);
    }
}
