#include "RenderPass.h"


RenderPass::RenderPass(std::shared_ptr<Instance> instance,
	std::shared_ptr<Device> device,
	std::shared_ptr<Surface> surface,
	std::shared_ptr<SwapChain> swapchain) :mInstance(instance), mDevice(device), mSurface(surface), mSwapChain(swapchain)
{

}

RenderPass::~RenderPass()
{
    vkDestroyRenderPass(mDevice->get(), mRenderpass, nullptr);
}
