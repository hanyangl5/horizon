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
	mPipeline = new Pipeline(mDevice, mSwapChain, mRenderPass);

	prepareAssests();
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
	mScene->loadModel("C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/DamagedHelmet/DamagedHelmet.gltf");
	mScene->addDirectLight(glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, glm::vec3(0.0f, 0.0f, -1.0f));
}

void Renderer::releaseAssets()
{

}
