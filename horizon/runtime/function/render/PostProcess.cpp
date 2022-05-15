#include "PostProcess.h"
#include <runtime/function/render/rhi/VulkanEnums.h>
#include <runtime/function/render/RenderContext.h>
#include <runtime/core/path/Path.h>

namespace Horizon
{
	PostProcess::PostProcess(std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept
	{
		CreateResources();

		//ComputePipelineCreateInfo m_tone_mapping_pass_create_info;
		//m_tone_mapping_pass_create_info.name = "transmittance_lut";
		//m_tone_mapping_pass_create_info.cs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("postprocess/tonemapping.comp.spv"));
		//m_tone_mapping_pass_create_info.descriptor_layouts = tone_mapping_descriptor_set_layouts;
		//m_tone_mapping_pass_create_info.group_count_x = _render_context.width / 8;
		//m_tone_mapping_pass_create_info.group_count_y = _render_context.height / 8;
		//m_tone_mapping_pass_create_info.group_count_z = 1;

		//m_tone_mapping_pass = _pipeline_manager->CreateComputePipeline(m_tone_mapping_pass_create_info);

		// post process

		std::shared_ptr<DescriptorSetInfo> pp_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		pp_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER);
		m_pp_descriptorset = std::make_shared<DescriptorSet>(_device, pp_descriptor_set_create_info);
		std::shared_ptr<DescriptorSetLayouts> pp_descriptor_set_layout = std::make_shared<DescriptorSetLayouts>();
		pp_descriptor_set_layout->layouts.push_back(m_pp_descriptorset->GetLayout());

		GraphicsPipelineCreateInfo pp_ipeline_create_info;
		pp_ipeline_create_info.name = "pp";
		pp_ipeline_create_info.vs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("postprocess.vert.spv"));
		pp_ipeline_create_info.ps = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("postprocess.frag.spv"));
		pp_ipeline_create_info.descriptor_layouts = pp_descriptor_set_layout;

		std::vector<AttachmentCreateInfo> pp_attachment_create_info{
			{TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM, COLOR_ATTACHMENT, TextureType::TEXTURE_TYPE_2D, _render_context.width, _render_context.height},
		};
		m_pipeline = _pipeline_manager->CreateGraphicsPipeline(pp_ipeline_create_info, pp_attachment_create_info, _render_context);
	}

	PostProcess::~PostProcess() noexcept
	{
	}

	void PostProcess::UpdateDescriptorSets() noexcept
	{
		m_pp_descriptorset->UpdateDescriptorSet(m_descriptor_set_update_desc);
	}

	void PostProcess::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept
	{
		m_descriptor_set_update_desc.BindResource(binding, buffer);
	}

	std::shared_ptr<AttachmentDescriptor> PostProcess::GetFrameBufferAttachment(u32 _index) const noexcept
	{
		return std::static_pointer_cast<GraphicsPipeline>(m_pipeline)->GetFrameBufferAttachment(_index);
	}

	std::shared_ptr<DescriptorSet> PostProcess::GetDescriptorSet() const noexcept
	{
		return m_pp_descriptorset;
	}

	std::shared_ptr<Pipeline> PostProcess::GetPipeline() const noexcept
	{
		return m_pipeline;
	}

	void PostProcess::CreateResources() noexcept
	{
		// tone mapping

		//std::shared_ptr<DescriptorSetInfo> tone_mapping_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		//tone_mapping_descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, SHADER_STAGE_COMPUTE_SHADER);

		//m_tone_mapping_descriptor_set = std::make_shared<DescriptorSet>(_device, tone_mapping_descriptor_set_create_info);

		//tone_mapping_descriptor_set_layouts = std::make_shared<DescriptorSetLayouts>();
		//tone_mapping_descriptor_set_layouts->layouts.push_back(m_tone_mapping_descriptor_set->GetLayout());
	}
	
}