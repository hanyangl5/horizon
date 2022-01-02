#include "Renderer.h"
#include <iostream>
class Window;

Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window)
{
	//mCamera = std::make_shared<Camera>();
	mInstance = std::make_shared<Instance>();
	mSurface = std::make_shared<Surface>(mInstance, window);
	mDevice = std::make_shared<Device>(mInstance, mSurface);
	mSwapChain = std::make_shared<SwapChain>(mDevice, mSurface, window);
	mPipeline = std::make_shared<Pipeline>(mInstance, mDevice, mSurface, mSwapChain);
	mFramebuffers = std::make_shared<Framebuffers>(mDevice, mSwapChain, mPipeline);
}

Renderer::~Renderer()
{

}

void Renderer::Update() {
	// clean up 
}
void Renderer::Render() {

}
