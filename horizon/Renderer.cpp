#include "Renderer.h"
#include <iostream>

namespace Horizon {

	class Window;

	Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window) :mWindow(window)
	{

		mInstance = std::make_shared<Instance>();
		mSurface = std::make_shared<Surface>(mInstance, mWindow);
		mDevice = std::make_shared<Device>(mInstance, mSurface);

		mRenderContext.width = width;
		mRenderContext.height = height;

		mSwapChain = std::make_shared<SwapChain>(mRenderContext, mDevice, mSurface);
		mCommandBuffer = std::make_shared<CommandBuffer>(mRenderContext, mDevice);
		mScene = std::make_shared<Scene>(mRenderContext, mDevice, mCommandBuffer);
		mFullscreenTriangle = std::make_shared<FullscreenTriangle>(mDevice, mCommandBuffer);
		mPipelineMgr = std::make_shared<PipelineManager>(mDevice);
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

		auto lightingPipeline = mPipelineMgr->get("lighting");

		presentDescriptorSet->allocateDescriptorSet();

		DescriptorSetUpdateDesc desc;
		desc.bindResource(0, lightingPipeline->getFramebufferDescriptorImageInfo(0));
		presentDescriptorSet->updateDescriptorSet(desc);

	}
	void Renderer::Render() {

		drawFrame();
		mCommandBuffer->submit(mSwapChain);
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
		for (u32 i = 0; i < mRenderContext.swapChainImageCount; i++)
		{
			mCommandBuffer->beginCommandRecording(i);

			auto lightingPipeline = mPipelineMgr->get("lighting");
			mCommandBuffer->beginRenderPass(i, lightingPipeline, false);
			mScene->draw(mCommandBuffer->get(i), lightingPipeline); // default shading pipeline
			mCommandBuffer->endRenderPass(i);

			auto presentPipeline = mPipelineMgr->get("present");
			mCommandBuffer->beginRenderPass(i, presentPipeline, true);
			mFullscreenTriangle->draw(mCommandBuffer->get(i), presentPipeline, { presentDescriptorSet }, true);
			mCommandBuffer->endRenderPass(i);

			mCommandBuffer->endCommandRecording(i);

		}
	}

	void Renderer::prepareAssests()
	{
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/DamagedHelmet/DamagedHelmet.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/plane.gltf");
		mScene->loadModel(getModelPath() + "earth6378/earth.gltf");
		//mScene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/earth_original_size/earth.gltf");
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

		std::shared_ptr<Shader> lightingVs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "defaultlit.vert.spv");
		std::shared_ptr<Shader> lightingPs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "defaultlit.frag.spv");

		PipelineCreateInfo lightingPipelineCreateInfo;

		lightingPipelineCreateInfo.vs = lightingVs;
		lightingPipelineCreateInfo.ps = lightingPs;
		lightingPipelineCreateInfo.descriptorLayouts = mScene->getDescriptorLayouts();

		// color + depth
		std::vector<AttachmentCreateInfo> lightingAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, mRenderContext.width, mRenderContext.height },
			{VK_FORMAT_D32_SFLOAT, DEPTH_STENCIL_ATTACHMENT, mRenderContext.width, mRenderContext.height}
		};


		mPipelineMgr->createPipeline(lightingPipelineCreateInfo, "lighting", lightingAttachmentsCreateInfo, mRenderContext);

#pragma region COMMENTED

		// scattering pass

		//std::shared_ptr<DescriptorSetInfo> scatterDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		//scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); // color tex
		//scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); // scatter params
		//std::shared_ptr<DescriptorSet> scatterDescriptorSet = std::make_shared<DescriptorSet>(mDevice, scatterDescriptorSetCreateInfo);
		//
		//std::shared_ptr<DescriptorSetLayouts> scatterDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		//scatterDescriptorSetLayout->layouts.push_back(scatterDescriptorSet->getLayout());


		//std::shared_ptr<Shader> scatterVs = std::make_shared<Shader>(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/spirv/scatter.vert.spv");
		//std::shared_ptr<Shader> scatterPs = std::make_shared<Shader>(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/spirv/scatter.frag.spv");
		//  
		//PipelineCreateInfo scatterPipelineCreateInfo;
		//scatterPipelineCreateInfo.vs = scatterVs;
		//scatterPipelineCreateInfo.ps = scatterPs;
		//scatterPipelineCreateInfo.descriptorLayouts = scatterDescriptorSetLayout;

		//std::vector<AttachmentCreateInfo> scatterAttachmentsCreateInfo;

		//mPipelineMgr->createPipeline(scatterPipelineCreateInfo, "scatter", scatterAttachmentsCreateInfo, mWidth, mHeight);



		// post process

		//std::shared_ptr<DescriptorSetInfo> ppDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		//ppDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		//std::shared_ptr<DescriptorSet> ppDescriptorSet = std::make_shared<DescriptorSet>(mDevice, ppDescriptorSetCreateInfo);
		//std::shared_ptr<DescriptorSetLayouts> ppDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		//ppDescriptorSetLayout->layouts.push_back(ppDescriptorSet->getLayout());


		//std::shared_ptr<Shader> ppVs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "postprocess.vert.spv");
		//std::shared_ptr<Shader> ppPs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "postprocess.frag.spv");

		//PipelineCreateInfo ppPipelineCreateInfo;
		//ppPipelineCreateInfo.vs = ppVs;
		//ppPipelineCreateInfo.ps = ppPs;
		//ppPipelineCreateInfo.descriptorLayouts = nullptr;

		//std::vector<AttachmentCreateInfo> ppAttachmentsCreateInfo{
		//	{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT | PRESENT_SRC, mWidth, mHeight},
		//	{VK_FORMAT_D32_SFLOAT, DEPTH_STENCIL_ATTACHMENT | PRESENT_SRC, mWidth, mHeight}
		//};
		//mPipelineMgr->createPipeline(ppPipelineCreateInfo, "pp", ppAttachmentsCreateInfo, mWidth, mHeight);

#pragma endregion

		// present pass

		std::shared_ptr<DescriptorSetInfo> presentDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		presentDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		presentDescriptorSet = std::make_shared<DescriptorSet>(mDevice, presentDescriptorSetCreateInfo);
		std::shared_ptr<DescriptorSetLayouts> presentDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		presentDescriptorSetLayout->layouts.push_back(presentDescriptorSet->getLayout());


		std::shared_ptr<Shader> presentVs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "present.vert.spv");
		std::shared_ptr<Shader> presentPs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "present.frag.spv");

		PipelineCreateInfo presentPipelineCreateInfo;
		presentPipelineCreateInfo.vs = presentVs;
		presentPipelineCreateInfo.ps = presentPs;
		presentPipelineCreateInfo.descriptorLayouts = presentDescriptorSetLayout;

		std::vector<AttachmentCreateInfo> presentAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT | PRESENT_SRC, mRenderContext.width, mRenderContext.height}
		};
		mPipelineMgr->createPresentPipeline(presentPipelineCreateInfo, presentAttachmentsCreateInfo, mRenderContext, mSwapChain);

		//mPipelineMgr->get("present")->attachToSwapChain(mSwapChain);
	}
}