#include "Renderer.h"
#include <iostream>
class Window;
Renderer::Renderer(u32 width, u32 height, Window* window) :mWindow(window)
{

}

Renderer::~Renderer()
{
	releaseAssets();

	delete mPipeline;
	delete mCommandBuffer;
	delete mFramebuffers;

	delete mRenderPass;
	delete mSwapChain;

	delete mSurface;
	delete mDevice;
	delete mInstance;
}

void Renderer::Init()
{
	//mCamera = std::make_shared<Camera>();
	mInstance = new Instance();
	mSurface = new Surface(mInstance, mWindow);
	mDevice = new Device(mInstance, mSurface);
	mSwapChain = new SwapChain(mDevice, mSurface, mWindow);
	mRenderPass = new RenderPass(mDevice, mSwapChain);
	mFramebuffers = new Framebuffers(mDevice, mSwapChain, mRenderPass);
	mCommandBuffer = new CommandBuffer(mDevice, mSwapChain, mFramebuffers);
	mScene = new Scene(mDevice, mCommandBuffer);
	mScene->loadModel("C:/Users/hylu/OneDrive/mycode/dredgen/resources/models/DamagedHelmet/DamagedHelmet.gltf");
	mPipeline = new Pipeline(mDevice, mSwapChain, mRenderPass);

}

void Renderer::Update() {
	mScene->prepare();
	mPipeline->create(mScene->getDescriptors());
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
	mScene->draw(mPipeline); // default shading pipeline
	mCommandBuffer->submit();
}

void Renderer::prepareAssests()
{

}

void Renderer::releaseAssets()
{

}
