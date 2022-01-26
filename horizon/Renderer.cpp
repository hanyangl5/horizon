#include "Renderer.h"
#include <iostream>
class Window;
Assest Renderer::mAssest;
Renderer::Renderer(u32 width, u32 height, Window* window) :mWindow(window)
{
	//mCamera = new Camera>();
	mInstance = new Instance();
	mSurface = new Surface(mInstance, mWindow);
	mDevice = new Device(mInstance, mSurface);
	mSwapChain = new SwapChain(mDevice, mSurface, mWindow);
	mPipeline = new Pipeline(mDevice, mSwapChain);
	mFramebuffers = new Framebuffers(mDevice, mSwapChain, mPipeline);
	mCommandBuffer = new CommandBuffer(mDevice, mSwapChain, mPipeline, mFramebuffers);
	prepareAssests();
	mCommandBuffer->beginCommandRecording(mAssest);
}

Renderer::~Renderer()
{
	delete mInstance;
	delete mSurface;
	delete mDevice;
	delete mSwapChain;
	delete mPipeline;
	delete mFramebuffers;
	delete mCommandBuffer;
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
	mAssest.prepare(mDevice, mCommandBuffer->getCommandpool());
}
