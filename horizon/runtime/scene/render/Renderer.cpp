#include "Renderer.h"

#include <iostream>
#include <runtime/core/math/Math.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/render/rhi/VulkanEnums.h>
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
		PrepareAssests();
		CreatePipelines();
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Init()
	{

	}

	void Renderer::Update() {
		m_scene->Prepare();

		auto& geometryPipeline = m_pipeline_manager->Get("geometry");
		auto& scatterPipeline = m_pipeline_manager->Get("scatter");
		auto& ppPipeline = m_pipeline_manager->Get("pp");

		m_scatter_descriptorSet->AllocateDescriptorSet();
		DescriptorSetUpdateDesc desc1;
		desc1.BindResource(0, geometryPipeline->GetFrameBufferAttachment(0));
		desc1.BindResource(1, geometryPipeline->GetFrameBufferAttachment(1));
		desc1.BindResource(2, geometryPipeline->GetFrameBufferAttachment(2));
		desc1.BindResource(3, m_scene->getCameraUbo());
		m_scatter_descriptorSet->UpdateDescriptorSet(desc1);

		m_pp_descriptorSet->AllocateDescriptorSet();
		DescriptorSetUpdateDesc desc2;
		desc2.BindResource(0, scatterPipeline->GetFrameBufferAttachment(0));
		m_pp_descriptorSet->UpdateDescriptorSet(desc2);

		m_present_descriptorSet->AllocateDescriptorSet();
		DescriptorSetUpdateDesc desc3;
		desc3.BindResource(0, ppPipeline->GetFrameBufferAttachment(0));
		m_present_descriptorSet->UpdateDescriptorSet(desc3);

	}
	void Renderer::Render() {

		DrawFrame();
		m_command_buffer->submit(m_swap_chain);
	}

	void Renderer::Wait()
	{
		vkDeviceWaitIdle(m_device->Get());
	}

	std::shared_ptr<Camera> Renderer::GetMainCamera() const
	{
		return m_scene->GetMainCamera();
	}

	void Renderer::DrawFrame()
	{
		for (u32 i = 0; i < m_render_context.swap_chain_image_count; i++)
		{
			m_command_buffer->beginCommandRecording(i);

			auto earth = m_scene->GetModel("earth");
			Math::mat4 scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(6378.0f));
			earth->SetModelMatrix(scaleMatrix);
			earth->UpdateModelMatrix();

			// geometry pass
			auto& geometryPipeline = m_pipeline_manager->Get("geometry");
			m_scene->Draw(i, m_command_buffer, geometryPipeline);


			scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(6478.0f));
			earth->SetModelMatrix(scaleMatrix);
			earth->UpdateModelMatrix();

			// scattering pass
			auto& scatteringPipeline = m_pipeline_manager->Get("scatter");

			m_scene->Draw(i, m_command_buffer, scatteringPipeline);

			// post process pass
			auto& ppPipeline = m_pipeline_manager->Get("pp");
			m_fullscreen_triangle->Draw(i, m_command_buffer, ppPipeline, { m_pp_descriptorSet });

			// final present pass
			auto& presentPipeline = m_pipeline_manager->Get("present");
			m_fullscreen_triangle->Draw(i, m_command_buffer, presentPipeline, { m_present_descriptorSet }, true);

			m_command_buffer->endCommandRecording(i);

		}
	}

	void Renderer::PrepareAssests()
	{
		m_scene->LoadModel(Path::GetInstance().GetModelPath("earth6378/earth.gltf"), "earth");
	}

	void Renderer::CreatePipelines()
	{

		PipelineCreateInfo geometryPipelineCreateInfo;

		geometryPipelineCreateInfo.vs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("geometry.vert.spv"));
		geometryPipelineCreateInfo.ps = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("geometry.frag.spv"));
		geometryPipelineCreateInfo.descriptor_layouts = m_scene->GetGeometryPassDescriptorLayouts();

		std::shared_ptr<PushConstants> geometryPipelinePushConstants = std::make_shared<PushConstants>();

		geometryPipelinePushConstants->pushConstantRanges = { {SHADER_STAGE_VERTEX_SHADER, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes 
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
		scatterDescriptorSetCreateInfo->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER); // positionDepth tex
		scatterDescriptorSetCreateInfo->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER); // normal r tex
		scatterDescriptorSetCreateInfo->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER); // albdeo m tex
		scatterDescriptorSetCreateInfo->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER); // camera params


		m_scatter_descriptorSet = std::make_shared<DescriptorSet>(m_device, scatterDescriptorSetCreateInfo);

		std::shared_ptr<DescriptorSetLayouts> scatterDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		scatterDescriptorSetLayout = m_scene->GetGeometryPassDescriptorLayouts();
		scatterDescriptorSetLayout->layouts.push_back(m_scatter_descriptorSet->GetLayout());

		std::shared_ptr<PushConstants> scatterPipelinePushConstants = std::make_shared<PushConstants>();
		scatterPipelinePushConstants->pushConstantRanges = { {SHADER_STAGE_VERTEX_SHADER, 0, 2 * sizeof(Math::mat4)} }; // Push constants have a minimum size of 128 bytes 

		PipelineCreateInfo scatterPipelineCreateInfo;
		scatterPipelineCreateInfo.vs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("scatter.vert.spv"));
		scatterPipelineCreateInfo.ps = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("scatter.frag.spv"));
		scatterPipelineCreateInfo.descriptor_layouts = scatterDescriptorSetLayout;
		scatterPipelineCreateInfo.pipeline_descriptor_set = m_scatter_descriptorSet;
		scatterPipelineCreateInfo.push_constants = scatterPipelinePushConstants;
		std::vector<AttachmentCreateInfo> scatterAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height }
		};

		m_pipeline_manager->createPipeline(scatterPipelineCreateInfo, "scatter", scatterAttachmentsCreateInfo, m_render_context);



		// post process

		std::shared_ptr<DescriptorSetInfo> ppDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		ppDescriptorSetCreateInfo->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER);
		m_pp_descriptorSet = std::make_shared<DescriptorSet>(m_device, ppDescriptorSetCreateInfo);
		std::shared_ptr<DescriptorSetLayouts> ppDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		ppDescriptorSetLayout->layouts.push_back(m_pp_descriptorSet->GetLayout());

		PipelineCreateInfo ppPipelineCreateInfo;
		ppPipelineCreateInfo.vs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("postprocess.vert.spv"));
		ppPipelineCreateInfo.ps = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("postprocess.frag.spv"));
		ppPipelineCreateInfo.descriptor_layouts = ppDescriptorSetLayout;

		std::vector<AttachmentCreateInfo> ppAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, m_render_context.width, m_render_context.height },
		};
		m_pipeline_manager->createPipeline(ppPipelineCreateInfo, "pp", ppAttachmentsCreateInfo, m_render_context);


		// present pass

		std::shared_ptr<DescriptorSetInfo> presentDescriptorSetCreateInfo = std::make_shared<DescriptorSetInfo>();
		presentDescriptorSetCreateInfo->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER);
		m_present_descriptorSet = std::make_shared<DescriptorSet>(m_device, presentDescriptorSetCreateInfo);
		std::shared_ptr<DescriptorSetLayouts> presentDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		presentDescriptorSetLayout->layouts.push_back(m_present_descriptorSet->GetLayout());

		std::shared_ptr<Shader> presentVs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("present.vert.spv"));
		std::shared_ptr<Shader> presentPs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("present.frag.spv"));

		PipelineCreateInfo presentPipelineCreateInfo;
		presentPipelineCreateInfo.vs = presentVs;
		presentPipelineCreateInfo.ps = presentPs;
		presentPipelineCreateInfo.descriptor_layouts = presentDescriptorSetLayout;
		std::vector<AttachmentCreateInfo> presentAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT | PRESENT_SRC, m_render_context.width, m_render_context.height}
		};
		m_pipeline_manager->createPresentPipeline(presentPipelineCreateInfo, presentAttachmentsCreateInfo, m_render_context, m_swap_chain);

	}
}