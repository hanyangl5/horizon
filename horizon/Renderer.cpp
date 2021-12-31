#include "Renderer.h"
#include <iostream>
class Window;

Renderer::Renderer(int width, int height, std::shared_ptr<Window> window)
{
	mCamera = std::make_shared<Camera>();
	mInstance = std::make_shared<Instance>();
	mSurface = std::make_shared<Surface>(mInstance,window);
	mDevice = std::make_shared<Device>(mInstance, mSurface);
	mSwapChain = std::make_shared<SwapChain>(mDevice, mSurface, window);
}

Renderer::~Renderer()
{

}

void Renderer::Update() {
	// clean up 
}
void Renderer::Render() {

}
