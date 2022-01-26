#include "Renderer.h"
#include <iostream>
class Window;
Assest Renderer::mAssest;
Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window) :mWindow(window)
{

}

Renderer::~Renderer()
{

}

void Renderer::Init()
{
	//mCamera = std::make_shared<Camera>();
	mInstance = std::make_shared<Instance>();
	mSurface = std::make_shared<Surface>(mInstance, mWindow);
	mDevice = std::make_shared<Device>(mInstance, mSurface);
	mSwapChain = std::make_shared<SwapChain>(mDevice, mSurface, mWindow);
	mDescriptrors = std::make_shared<Descriptors>(mDevice);

	DescriptorSetInfo setInfo;
	setInfo.addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	mDescriptrors->addDescriptorSet(&setInfo);
	mDescriptrors->createDescriptorSetLayout();
	mDescriptrors->createDescriptorPool();
	mDescriptrors->allocateDescriptors();

	mPipeline = std::make_shared<Pipeline>(mDevice, mSwapChain, mDescriptrors);
	mFramebuffers = std::make_shared<Framebuffers>(mDevice, mSwapChain, mPipeline);
	mCommandBuffer = std::make_shared<CommandBuffer>(mDevice, mSwapChain, mPipeline, mFramebuffers);
	prepareAssests();
}

void Renderer::Update() {
	
	testUBO->update(&colorubo,sizeof(colorubo));
	BufferDesc desc;
	desc.ubos.push_back(testUBO);
	mDescriptrors->updateDescriptorSet(0, &desc);
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
