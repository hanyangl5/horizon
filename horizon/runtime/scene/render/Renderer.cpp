#include "Renderer.h"

#include <iostream>
#include <runtime/core/math/Math.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/render/rhi/VulkanEnums.h>
namespace Horizon
{

	class Window;

	Renderer::Renderer(u32 width, u32 height, std::shared_ptr<Window> window) noexcept : m_window(window)
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

	Renderer::~Renderer() noexcept
	{
	}

	void Renderer::Init() noexcept
	{
	}

	void Renderer::Update() noexcept
	{
		m_scene->Prepare();

		m_scattering_pass->SetInvViewProjectionMatrix(m_scene->GetMainCamera()->GetInvViewProjectionMatrix());

		m_scattering_pass->BindResource(0, m_scene->getCameraUbo());
		m_scattering_pass->UpdateDescriptorSets();

		m_post_process_pass->BindResource(0, m_scattering_pass->GetFrameBufferAttachment(0));
		m_post_process_pass->UpdateDescriptorSets();

		m_present_descriptorSet->AllocateDescriptorSet();
		DescriptorSetUpdateDesc desc;
		desc.BindResource(0, m_post_process_pass->GetFrameBufferAttachment(0));
		m_present_descriptorSet->UpdateDescriptorSet(desc);
	}

	void Renderer::Render() noexcept
	{

		DrawFrame();
		m_command_buffer->submit(m_swap_chain);
	}

	void Renderer::Wait() noexcept
	{
		vkDeviceWaitIdle(m_device->Get());
	}

	std::shared_ptr<Camera> Renderer::GetMainCamera() const noexcept
	{
		return m_scene->GetMainCamera();
	}

	void Renderer::DrawFrame() noexcept
	{
		for (u32 i = 0; i < m_render_context.swap_chain_image_count; i++)
		{
			m_command_buffer->beginCommandRecording(i);

			// geometry pass
			//m_scene->Draw(i, m_command_buffer, m_geometry_pass->GetPipeline());
			// scattering pass
			m_fullscreen_triangle->Draw(i, m_command_buffer, m_scattering_pass->GetPipeline(), {m_scattering_pass->GetDescriptorSet()});
			// post process pass
			m_fullscreen_triangle->Draw(i, m_command_buffer, m_post_process_pass->GetPipeline(), {m_post_process_pass->GetDescriptorSet()});
			// final present pass
			m_fullscreen_triangle->Draw(i, m_command_buffer, m_pipeline_manager->Get("present"), {m_present_descriptorSet}, true);

			m_command_buffer->endCommandRecording(i);
		}
	}

	void Renderer::PrepareAssests() noexcept
	{
		m_scene->LoadModel(Path::GetInstance().GetModelPath("earth6378/earth.gltf"), "earth");

		auto earth = m_scene->GetModel("earth");
		Math::mat4 scaleMatrix = Math::scale(Math::mat4(1.0f), Math::vec3(6378.0f));
		earth->SetModelMatrix(scaleMatrix);
		earth->UpdateModelMatrix();
	}

	void Renderer::CreatePipelines() noexcept
	{

		//m_geometry_pass = std::make_shared<Geometry>(m_scene, m_pipeline_manager, m_device, m_render_context);

		m_scattering_pass = std::make_shared<Atmosphere>(m_pipeline_manager, m_device, m_render_context);

		m_post_process_pass = std::make_shared<PostProcess>(m_pipeline_manager, m_device, m_render_context);

		CreatePresentPipeline();
	}
	void Renderer::CreatePresentPipeline() noexcept
	{
		// present pass

		std::shared_ptr<DescriptorSetInfo> present_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		present_descriptor_set_create_info->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER);
		m_present_descriptorSet = std::make_shared<DescriptorSet>(m_device, present_descriptor_set_create_info);
		std::shared_ptr<DescriptorSetLayouts> presentDescriptorSetLayout = std::make_shared<DescriptorSetLayouts>();
		presentDescriptorSetLayout->layouts.push_back(m_present_descriptorSet->GetLayout());

		std::shared_ptr<Shader> presentVs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("present.vert.spv"));
		std::shared_ptr<Shader> presentPs = std::make_shared<Shader>(m_device->Get(), Path::GetInstance().GetShaderPath("present.frag.spv"));

		PipelineCreateInfo presentPipelineCreateInfo;
		presentPipelineCreateInfo.name = "present";
		presentPipelineCreateInfo.vs = presentVs;
		presentPipelineCreateInfo.ps = presentPs;
		presentPipelineCreateInfo.descriptor_layouts = presentDescriptorSetLayout;
		std::vector<AttachmentCreateInfo> presentAttachmentsCreateInfo{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT | PRESENT_SRC, m_render_context.width, m_render_context.height}};
		m_pipeline_manager->createPresentPipeline(presentPipelineCreateInfo, presentAttachmentsCreateInfo, m_render_context, m_swap_chain);
	}
}