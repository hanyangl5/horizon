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

		auto& geometryPipeline = mPipelineMgr->get("geometry");

		scatterDescriptorSet->allocateDescriptorSet();

		DescriptorSetUpdateDesc desc1;
		desc1.bindResource(0, geometryPipeline->getFramebufferDescriptorImageInfo(0));
		desc1.bindResource(1, geometryPipeline->getFramebufferDescriptorImageInfo(1));
		desc1.bindResource(2, geometryPipeline->getFramebufferDescriptorImageInfo(2));
		scatterDescriptorSet->updateDescriptorSet(desc1);

		auto& scatterPipeline = mPipelineMgr->get("scatter");

		presentDescriptorSet->allocateDescriptorSet();

		DescriptorSetUpdateDesc desc2;
		desc2.bindResource(0, scatterPipeline->getFramebufferDescriptorImageInfo(0));
		presentDescriptorSet->updateDescriptorSet(desc2);

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

			// geometry pass
			auto& geometryPipeline = mPipelineMgr->get("geometry");
			mCommandBuffer->beginRenderPass(i, geometryPipeline, false);
			mScene->draw(mCommandBuffer->get(i), geometryPipeline);
			mCommandBuffer->endRenderPass(i);

			// scattering pass
			auto& scatteringPipeline = mPipelineMgr->get("scatter");
			mCommandBuffer->beginRenderPass(i, scatteringPipeline, false);
			mScene->draw(mCommandBuffer->get(i), scatteringPipeline);
			mCommandBuffer->endRenderPass(i);

			// final present pass
			auto& presentPipeline = mPipelineMgr->get("present");
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
		mScene->loadModel(getModelPath() + "earth6378/earth.gltf", "earth");
		//Math::mat4 scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(1.0f/6378.0f));
		//mScene->getModel("earth")->setModelMatrix(scaleMatrix);
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

		PipelineCreateInfo geometryPipelineCreateInfo;

		geometryPipelineCreateInfo.vs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "geometry.vert.spv");
		geometryPipelineCreateInfo.ps = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "geometry.frag.spv");
		geometryPipelineCreateInfo.descriptorLayouts = mScene->getGeometryPassDescriptorLayouts();

		std::shared_ptr<PushConstants> geometryPipelinePushConstants = std::make_shared<PushConstants>();

		geometryPipelinePushConstants->pushConstantRanges = { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes 
		geometryPipelineCreateInfo.pushConstants = geometryPipelinePushConstants; 
		// position + depth
		// normal
		// albedo
		// depth

		std::vector<AttachmentCreateInfo> geometryAttachmentsCreateInfo{
			{VK_FORMAT_R32G32B32A32_SFLOAT, COLOR_ATTACHMENT, mRenderContext.width, mRenderContext.height },
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, mRenderContext.width, mRenderContext.height },
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, mRenderContext.width, mRenderContext.height },
			{VK_FORMAT_D32_SFLOAT, DEPTH_STENCIL_ATTACHMENT, mRenderContext.width, mRenderContext.height}
		};


		mPipelineMgr->createPipeline(geometryPipelineCreateInfo, "geometry", geometryAttachmentsCreateInfo, mRenderContext);

		// scattering pass

		std::shared_ptr<DescriptorSetInfo> scatterDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // positionDepth tex
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // normal r tex
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // albdeo m tex
		//scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); // scatter params

		scatterDescriptorSet = std::make_shared<DescriptorSet>(mDevice, scatterDescriptorSetCreateInfo);
		
		std::shared_ptr<DescriptorSetLayouts> scatterDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		scatterDescriptorSetLayout = mScene->getGeometryPassDescriptorLayouts();
		scatterDescriptorSetLayout->layouts.push_back(scatterDescriptorSet->getLayout());

		std::shared_ptr<PushConstants> scatterPipelinePushConstants = std::make_shared<PushConstants>();
		scatterPipelinePushConstants->pushConstantRanges = { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes 


		PipelineCreateInfo scatterPipelineCreateInfo;
		scatterPipelineCreateInfo.vs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "scatter.vert.spv");
		scatterPipelineCreateInfo.ps = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "scatter.frag.spv");
		scatterPipelineCreateInfo.descriptorLayouts = scatterDescriptorSetLayout;
		scatterPipelineCreateInfo.pipelineDescriptorSet = scatterDescriptorSet;
		scatterPipelineCreateInfo.pushConstants = scatterPipelinePushConstants;
		std::vector<AttachmentCreateInfo> scatterAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, mRenderContext.width, mRenderContext.height }
		};

		mPipelineMgr->createPipeline(scatterPipelineCreateInfo, "scatter", scatterAttachmentsCreateInfo, mRenderContext);



		//// post process

		//std::shared_ptr<DescriptorSetInfo> ppDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		//ppDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		//ppDescriptorSet = std::make_shared<DescriptorSet>(mDevice, ppDescriptorSetCreateInfo);
		//std::shared_ptr<DescriptorSetLayouts> ppDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		//ppDescriptorSetLayout->layouts.push_back(ppDescriptorSet->getLayout());

		//PipelineCreateInfo ppPipelineCreateInfo;
		//ppPipelineCreateInfo.vs = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "postprocess.vert.spv");
		//ppPipelineCreateInfo.ps = std::make_shared<Shader>(mDevice->get(), getShaderPath() + "postprocess.frag.spv");
		//ppPipelineCreateInfo.descriptorLayouts = nullptr;

		//std::vector<AttachmentCreateInfo> ppAttachmentsCreateInfo{
		//	{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, mRenderContext.width, mRenderContext.height },
		//};
		//mPipelineMgr->createPipeline(ppPipelineCreateInfo, "pp", ppAttachmentsCreateInfo, mRenderContext);


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