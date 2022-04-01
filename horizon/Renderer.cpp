#include "Renderer.h"
#include <iostream>

namespace Horizon {

	class Window;

	Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window) :mWindow(window), mWidth(width), mHeight(height)
	{
		mInstance = std::make_shared<Instance>();
		mSurface = std::make_shared<Surface>(mInstance, mWindow);
		mDevice = std::make_shared<Device>(mInstance, mSurface);
		mSwapChain = std::make_shared<SwapChain>(mDevice, mSurface, mWindow);
		mCommandBuffer = std::make_shared<CommandBuffer>(mDevice, mSwapChain);
		mScene = std::make_shared<Scene>(mDevice, mCommandBuffer, mWidth, mHeight);
		mPipelineMgr = std::make_shared<PipelineManager>(mDevice, mSwapChain);
		prepareAssests();
		createPipelines();
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Init()
	{

	}

	void Renderer::Update() {
		mScene->prepare();
	}
	void Renderer::Render() {

		drawFrame();
	}

	void Renderer::wait()
	{
		vkDeviceWaitIdle(mDevice->get());
	}

	std::shared_ptr<Camera> Renderer::getMainCamera() const
	{
		return mScene->getMainCamera();
	}

	void Renderer::drawFrame()
	{
		mScene->draw(mPipelineMgr->get("lighting")); // default shading pipeline
		
		mCommandBuffer->submit();
	}

	void Renderer::prepareAssests()
	{
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/DamagedHelmet/DamagedHelmet.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/plane.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/earth/earth.gltf");
		mScene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/earth_original_size/earth.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/vulkan_asset_pack_gltf/data/models/FlightHelmet/glTF/FlightHelmet.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/gltf/2.0/TwoSidedPlane/glTF/TwoSidedPlane.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/gltf/2.0/Sponza/glTF/Sponza.gltf");

		mScene->addDirectLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(0.0f, -1.0f, -1.0f));
		//mScene->addPointLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(5.0f, 1.0f, 0.0f), 20.0f);
		//mScene->addSpotLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(-1.0f, -1.0f, 0.0f),  Math::vec3(-1.0f, 1.0f, 0.0f), 10.0f, Math::radians(0.0f),Math::radians(90.0f));
		//mScene->addSpotLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(0.0f, -1.0f, 0.0f),  Math::vec3(1.0f, 1.0f, 0.0f), 10.0f, Math::radians(0.0f),Math::radians(45.0f));

	}

	void Renderer::createPipelines()
	{

		std::shared_ptr<Shader> lightingVs=std::make_shared<Shader>(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/spirv/defaultlit.vert.spv");
		std::shared_ptr<Shader> lightingPs=std::make_shared<Shader>(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/spirv/defaultlit.frag.spv");

		std::shared_ptr<PipelineCreateInfo> lightingPipelineCreateInfo = std::make_shared<PipelineCreateInfo>();

		lightingPipelineCreateInfo->vs = lightingVs;
		lightingPipelineCreateInfo->ps = lightingPs;
		lightingPipelineCreateInfo->descriptorLayouts = mScene->getDescriptorLayouts();
		
		mPipelineMgr->createPipeline(lightingPipelineCreateInfo, "lighting");

		// post process

		//std::shared_ptr<DescriptorSetInfo> ppDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		//ppDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		//std::shared_ptr<DescriptorSet> ppDescriptorSet = std::make_shared<DescriptorSet>(mDevice, ppDescriptorSetCreateInfo);
		//std::shared_ptr<DescriptorSetLayouts> ppDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		//ppDescriptorSetLayout->layouts.push_back(ppDescriptorSet->getLayout());


		//std::shared_ptr<Shader> ppVs = std::make_shared<Shader>(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/spirv/postprocess.vert.spv");
		//std::shared_ptr<Shader> ppPs = std::make_shared<Shader>(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/spirv/postprocess.frag.spv");

		//std::shared_ptr<PipelineCreateInfo> ppPipelineCreateInfo = std::make_shared<PipelineCreateInfo>();
		//ppPipelineCreateInfo->vs = ppVs;
		//ppPipelineCreateInfo->ps = ppPs;
		//ppPipelineCreateInfo->descriptorLayouts = ppDescriptorSetLayout;

		//mPipelineMgr->createPipeline(ppPipelineCreateInfo, "pp");

	}
}