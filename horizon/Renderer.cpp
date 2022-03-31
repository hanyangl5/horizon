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

		delete mCommandBuffer;
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
		mCommandBuffer = new CommandBuffer(mDevice, mSwapChain);
		mScene = new Scene(mDevice, mCommandBuffer, mWidth, mHeight);
		mPipelineMgr.init(mDevice, mSwapChain);
		prepareAssests();
		createPipelines();
	}

	void Renderer::Update() {
		mScene->prepare();
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
		mScene->draw(mPipelineMgr.get("lighting")); // default shading pipeline

		mCommandBuffer->submit();
	}

	void Renderer::prepareAssests()
	{
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/DamagedHelmet/DamagedHelmet.gltf");
		mScene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/vulkan_asset_pack_gltf/data/models/FlightHelmet/glTF/FlightHelmet.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/gltf/2.0/TwoSidedPlane/glTF/TwoSidedPlane.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/plane.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/gltf/2.0/Sponza/glTF/Sponza.gltf");

		mScene->addDirectLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(0.0f, -1.0f, -1.0f));
		mScene->addPointLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(5.0f, 1.0f, 0.0f), 20.0f);
		mScene->addSpotLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(-1.0f, -1.0f, 0.0f),  Math::vec3(-1.0f, 1.0f, 0.0f), 10.0f, Math::radians(0.0f),Math::radians(90.0f));
		mScene->addSpotLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(0.0f, -1.0f, 0.0f),  Math::vec3(1.0f, 1.0f, 0.0f), 10.0f, Math::radians(0.0f),Math::radians(45.0f));

	}

	void Renderer::releaseAssets()
	{
		delete mScene;
	}
	void Renderer::createPipelines()
	{

		Shader lightingVs(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/defaultlit.vert.spv");
		Shader lightingPs(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/defaultlit.frag.spv");


		PipelineCreateInfo lightingPipelineCreateInfo;
		lightingPipelineCreateInfo.vs = &lightingVs;
		lightingPipelineCreateInfo.ps = &lightingPs;
		lightingPipelineCreateInfo.descriptorLayouts = mScene->getDescriptorLayouts();
		
		mPipelineMgr.createPipeline(&lightingPipelineCreateInfo, "lighting");
	}
}