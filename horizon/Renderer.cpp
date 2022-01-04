#include "Renderer.h"
#include <iostream>
class Window;
Assest Renderer::mAssest;
Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window):mWindow(window)
{
	//mCamera = std::make_shared<Camera>();
	mInstance = std::make_shared<Instance>();
	mSurface = std::make_shared<Surface>(mInstance, mWindow);
	mDevice = std::make_shared<Device>(mInstance, mSurface);
	mSwapChain = std::make_shared<SwapChain>(mDevice, mSurface, mWindow);
	mPipeline = std::make_shared<Pipeline>(mDevice, mSwapChain);
	mFramebuffers = std::make_shared<Framebuffers>(mDevice, mSwapChain, mPipeline);
	prepareAssests();
	mCommandBuffer = std::make_shared<CommandBuffer>(mDevice, mSwapChain,mPipeline,mFramebuffers,mAssest);
}

Renderer::~Renderer()
{

}

void Renderer::Update() {
	// clean up 
}
void Renderer::Render() {
	drawFrame();
}

void Renderer::wait()
{
	vkDeviceWaitIdle(mDevice->get());
}

void Renderer::drawFrame()
{
	mCommandBuffer->draw();
}

void Renderer::prepareAssests()
{
	mAssest.prepare(mDevice);
}
