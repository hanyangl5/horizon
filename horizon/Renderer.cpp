#include "Renderer.h"
#include <iostream>

namespace Horizon {

	class Window;

	Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window) :m_window(window)
	{

		m_instance = std::make_shared<Instance>();
		m_surface = std::make_shared<Surface>(m_instance, m_window);
		m_device = std::make_shared<Device>(m_instance, m_surface);

		m_render_context.width = width;
		m_render_context.height = height;

		m_swap_chain = std::make_shared<SwapChain>(m_render_context, m_device, m_surface);
		m_command_buffer = std::make_shared<CommandBuffer>(m_render_context, m_device);
		m_scene = std::make_shared<Scene>(m_render_context, m_device, m_command_buffer);
		m_fullscreen_triangle = std::make_shared<FullscreenTriangle>(m_device, m_command_buffer);
		m_pipeline_manager = std::make_shared<PipelineManager>(m_device);
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
		m_scene->prepare();

		auto& geometryPipeline = m_pipeline_manager->get("geometry");

		m_scatter_descriptorSet->allocateDescriptorSet();
		
		DescriptorSetUpdateDesc desc1;
		desc1.bindResource(0, geometryPipeline->getFramebufferDescriptorImageInfo(0));
		desc1.bindResource(1, geometryPipeline->getFramebufferDescriptorImageInfo(1));
		desc1.bindResource(2, geometryPipeline->getFramebufferDescriptorImageInfo(2));
		desc1.bindResource(3, m_scene->getCameraUbo());
		m_scatter_descriptorSet->UpdateDescriptorSet(desc1);

		auto& scatterPipeline = m_pipeline_manager->get("scatter");

		m_present_descriptorSet->allocateDescriptorSet();

		DescriptorSetUpdateDesc desc2;
		desc2.bindResource(0, scatterPipeline->getFramebufferDescriptorImageInfo(0));
		m_present_descriptorSet->UpdateDescriptorSet(desc2);

	}
	void Renderer::Render() {

		drawFrame();
		m_command_buffer->submit(m_swap_chain);
	}

	void Renderer::wait()
	{
		vkDeviceWaitIdle(m_device->get());
	}

	std::shared_ptr<Camera> Renderer::getMainCamera() const
	{
		return m_scene->getMainCamera();
	}

	void Renderer::drawFrame()
	{
		for (u32 i = 0; i < m_render_context.swap_chain_image_count; i++)
		{
			m_command_buffer->beginCommandRecording(i);

			auto earth = m_scene->getModel("earth");
			Math::mat4 scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(6378.0f));
			earth->setModelMatrix(scaleMatrix);
			earth->updateModelMatrix();

			// geometry pass
			auto& geometryPipeline = m_pipeline_manager->get("geometry");
			m_command_buffer->beginRenderPass(i, geometryPipeline, false);
			m_scene->draw(m_command_buffer->get(i), geometryPipeline);
			m_command_buffer->endRenderPass(i);

			scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(8378.0f));
			earth->setModelMatrix(scaleMatrix);
			earth->updateModelMatrix();

			// scattering pass
			auto& scatteringPipeline = m_pipeline_manager->get("scatter");
			m_command_buffer->beginRenderPass(i, scatteringPipeline, false);
			m_scene->draw(m_command_buffer->get(i), scatteringPipeline);
			m_command_buffer->endRenderPass(i);

			// final present pass
			auto& presentPipeline = m_pipeline_manager->get("present");
			m_command_buffer->beginRenderPass(i, presentPipeline, true);
			m_fullscreen_triangle->draw(m_command_buffer->get(i), presentPipeline, { m_present_descriptorSet }, true);
			m_command_buffer->endRenderPass(i);

			m_command_buffer->endCommandRecording(i);

		}
	}

	void Renderer::prepareAssests()
	{
		//m_scene->loadModel("C:/Users/hylu/OneDrive/mycode/DredgenGraphicEngine/Dredgen-gl/resources/models/DamagedHelmet/DamagedHelmet.gltf");
		//m_scene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/plane.gltf");
		m_scene->loadModel(getModelPath() + "earth6378/earth.gltf", "earth");
		//Math::mat4 scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(1.0f/6378.0f));
		//m_scene->getModel("earth")->setModelMatrix(scaleMatrix);
		//m_scene->loadModel("C:/Users/hylu/OneDrive/mycode/vulkan/data/earth_original_size/earth.gltf");
		//m_scene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/vulkan_asset_pack_gltf/data/models/FlightHelmet/glTF/FlightHelmet.gltf");
		//m_scene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/gltf/2.0/TwoSidedPlane/glTF/TwoSidedPlane.gltf");
		//m_scene->loadModel("C:/Users/hylu/OneDrive/Program/Computer Graphics/models/gltf/2.0/Sponza/glTF/Sponza.gltf");

		m_scene->addDirectLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(0.0f, -1.0f, -1.0f));
		//m_scene->addPointLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(5.0f, 1.0f, 0.0f), 20.0f);
		//m_scene->addSpotLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(-1.0f, -1.0f, 0.0f),  Math::vec3(-1.0f, 1.0f, 0.0f), 10.0f, Math::radians(0.0f),Math::radians(90.0f));
		//m_scene->addSpotLight(Math::vec3(1.0f, 1.0f, 1.0f), 1.0f, Math::vec3(0.0f, -1.0f, 0.0f),  Math::vec3(1.0f, 1.0f, 0.0f), 10.0f, Math::radians(0.0f),Math::radians(45.0f));

	}

	void Renderer::createPipelines()
	{

		PipelineCreateInfo geometryPipelineCreateInfo;

		geometryPipelineCreateInfo.vs = std::make_shared<Shader>(m_device->get(), getShaderPath() + "geometry.vert.spv");
		geometryPipelineCreateInfo.ps = std::make_shared<Shader>(m_device->get(), getShaderPath() + "geometry.frag.spv");
		geometryPipelineCreateInfo.descriptor_layouts = m_scene->getGeometryPassDescriptorLayouts();

		std::shared_ptr<PushConstants> geometryPipelinePushConstants = std::make_shared<PushConstants>();

		geometryPipelinePushConstants->pushConstantRanges = { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes 
		geometryPipelineCreateInfo.push_constants = geometryPipelinePushConstants; 
		// position + depth
		// normal
		// albedo
		// depth

		std::vector<AttachmentCreateInfo> geometryAttachmentsCreateInfo{
			{VK_FORMAT_R32G32B32A32_SFLOAT, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height },
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height },
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height },
			{VK_FORMAT_D32_SFLOAT, DEPTH_STENCIL_ATTACHMENT, m_render_context.width, m_render_context.height}
		};


		m_pipeline_manager->createPipeline(geometryPipelineCreateInfo, "geometry", geometryAttachmentsCreateInfo, m_render_context);

		// scattering pass

	
		std::shared_ptr<DescriptorSetInfo> scatterDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // positionDepth tex
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // normal r tex
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // albdeo m tex
		//scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT); // scatter params
		scatterDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT); // camera params


		m_scatter_descriptorSet = std::make_shared<DescriptorSet>(m_device, scatterDescriptorSetCreateInfo);
		
		std::shared_ptr<DescriptorSetLayouts> scatterDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		scatterDescriptorSetLayout = m_scene->getGeometryPassDescriptorLayouts();
		scatterDescriptorSetLayout->layouts.push_back(m_scatter_descriptorSet->getLayout());

		std::shared_ptr<PushConstants> scatterPipelinePushConstants = std::make_shared<PushConstants>();
		scatterPipelinePushConstants->pushConstantRanges = { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes 


		PipelineCreateInfo scatterPipelineCreateInfo;
		scatterPipelineCreateInfo.vs = std::make_shared<Shader>(m_device->get(), getShaderPath() + "scatter.vert.spv");
		scatterPipelineCreateInfo.ps = std::make_shared<Shader>(m_device->get(), getShaderPath() + "scatter.frag.spv");
		scatterPipelineCreateInfo.descriptor_layouts = scatterDescriptorSetLayout;
		scatterPipelineCreateInfo.pipeline_descriptor_set = m_scatter_descriptorSet;
		scatterPipelineCreateInfo.push_constants = scatterPipelinePushConstants;
		std::vector<AttachmentCreateInfo> scatterAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height }
		};

		m_pipeline_manager->createPipeline(scatterPipelineCreateInfo, "scatter", scatterAttachmentsCreateInfo, m_render_context);



		//// post process

		//std::shared_ptr<DescriptorSetInfo> ppDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		//ppDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		//m_pp_descriptorSet = std::make_shared<DescriptorSet>(m_device, ppDescriptorSetCreateInfo);
		//std::shared_ptr<DescriptorSetLayouts> ppDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		//ppDescriptorSetLayout->layouts.push_back(m_pp_descriptorSet->getLayout());

		//PipelineCreateInfo ppPipelineCreateInfo;
		//ppPipelineCreateInfo.vs = std::make_shared<Shader>(m_device->get(), getShaderPath() + "postprocess.vert.spv");
		//ppPipelineCreateInfo.ps = std::make_shared<Shader>(m_device->get(), getShaderPath() + "postprocess.frag.spv");
		//ppPipelineCreateInfo.descriptor_layouts = nullptr;

		//std::vector<AttachmentCreateInfo> ppAttachmentsCreateInfo{
		//	{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height },
		//};
		//m_pipeline_manager->createPipeline(ppPipelineCreateInfo, "pp", ppAttachmentsCreateInfo, m_render_context);


		// present pass

		std::shared_ptr<DescriptorSetInfo> presentDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		presentDescriptorSetCreateInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_present_descriptorSet = std::make_shared<DescriptorSet>(m_device, presentDescriptorSetCreateInfo);
		std::shared_ptr<DescriptorSetLayouts> presentDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		presentDescriptorSetLayout->layouts.push_back(m_present_descriptorSet->getLayout());


		std::shared_ptr<Shader> presentVs = std::make_shared<Shader>(m_device->get(), getShaderPath() + "present.vert.spv");
		std::shared_ptr<Shader> presentPs = std::make_shared<Shader>(m_device->get(), getShaderPath() + "present.frag.spv");

		PipelineCreateInfo presentPipelineCreateInfo;
		presentPipelineCreateInfo.vs = presentVs;
		presentPipelineCreateInfo.ps = presentPs;
		presentPipelineCreateInfo.descriptor_layouts = presentDescriptorSetLayout;
		std::vector<AttachmentCreateInfo> presentAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT | PRESENT_SRC, m_render_context.width, m_render_context.height}
		};
		m_pipeline_manager->createPresentPipeline(presentPipelineCreateInfo, presentAttachmentsCreateInfo, m_render_context, m_swap_chain);

		//m_pipeline_manager->get("present")->attachToSwapChain(m_swap_chain);
	}
}