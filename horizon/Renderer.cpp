#include "Renderer.h"
#include <iostream>

namespace Horizon {

	class Window;

	Renderer::Renderer(u32 width, u32 height, Window* window) :mWindow(window), mWidth(width), mHeight(height)
	{

	}

	Renderer::~Renderer()
	{
		releaseAssets();

		//delete mScene;
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
		mInstance = new Instance();
		mSurface = new Surface(mInstance, mWindow);
		mDevice = new Device(mInstance, mSurface);
		mSwapChain = new SwapChain(mDevice, mSurface, mWindow);
		mRenderPass = new RenderPass(mDevice, mSwapChain);
		mFramebuffers = new Framebuffers(mDevice, mSwapChain, mRenderPass);
		mCommandBuffer = new CommandBuffer(mDevice, mSwapChain, mFramebuffers);
		mScene = new Scene(mDevice, mCommandBuffer, mWidth, mHeight);
		mPipeline = new Pipeline(mDevice, mSwapChain, mRenderPass);

		prepareAssests();
	}

	void Renderer::Update() {
		mScene->prepare();
		// TODO: pipeline map
		//if (mPipeline->get()&&mPipeline->getLayout()) {
		//	mPipeline->destroy();
		//}
		mPipeline->create(mScene->getDescriptors());
	}
	void Renderer::Render() {

		drawFrame();
	}

	void Renderer::Destroy()
	{
	}

	void Renderer::wait()
	{
		vkDeviceWaitIdle(mDevice->get());
	}

	Camera* Renderer::getMainCamera() const
	{
		return mScene->getMainCamera();
	}

	void Renderer::drawFrame()
	{
		mScene->draw(mPipeline); // default shading pipeline
		mCommandBuffer->submit();
	}

	void Renderer::prepareAssests()
	{
		mScene->loadModel("C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/DamagedHelmet/DamagedHelmet.gltf");
		mScene->addDirectLight(vec3(1.0f, 1.0f, 1.0f), 1.0f, vec3(0.0f, 0.0f, -1.0f));
	}

	void Renderer::releaseAssets()
	{
		delete mScene;
	}
}