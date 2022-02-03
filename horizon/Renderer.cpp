#include "Renderer.h"
#include <iostream>
class Window;
Assest Renderer::mAssest;
Renderer::Renderer(u32 width, u32 height, Window* window) :mWindow(window)
{

}

Renderer::~Renderer()
{
	releaseAssets();
	delete mCommandBuffer;
	delete mFramebuffers;
	delete mPipeline;
	delete mDescriptors;
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
	mDescriptors = new Descriptors(mDevice);

	DescriptorSetInfo setInfo;
	setInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	mDescriptors->addDescriptorSet(&setInfo);
	mDescriptors->createDescriptorSetLayout();
	mDescriptors->createDescriptorPool();
	mDescriptors->allocateDescriptors();

	mPipeline = new Pipeline(mDevice, mSwapChain, mDescriptors);
	mFramebuffers = new Framebuffers(mDevice, mSwapChain, mPipeline);
	mCommandBuffer = new CommandBuffer(mDevice, mSwapChain, mPipeline, mFramebuffers);
	prepareAssests();
}

void Renderer::Update() {
	BufferDesc bufferdesc;
	testUBO->update(&colorubo, sizeof(colorubo));
	bufferdesc.ubos.push_back(testUBO);
	mDescriptors->updateDescriptorSet(0, &bufferdesc);
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

	mCommandBuffer->draw(mAssest);
	mCommandBuffer->submit();
}

void Renderer::prepareAssests()
{
	mAssest.prepare(mDevice, mCommandBuffer->getCommandpool());
	testUBO = new UniformBuffer(mDevice);
}

void Renderer::releaseAssets()
{
	delete testUBO;
	mAssest.ReleaseBuffer();
}
