#include "Framebuffers.h"

Framebuffers::Framebuffers(Device* device,SwapChain* swapchain,Pipeline* pipeline):mDevice(device),mSwapChain(swapchain),mPipeline(pipeline)
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

VkFramebuffer Framebuffers::get(u32 i) const
{
    return mSwapChainFrameBuffers[i];
}

void Framebuffers::createFrameBuffers()
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
