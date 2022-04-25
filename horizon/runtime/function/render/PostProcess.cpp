#include "PostProcess.h"
#include <runtime/function/render/rhi/VulkanEnums.h>
#include <runtime/function/render/RenderContext.h>
#include <runtime/core/path/Path.h>

namespace Horizon
{
	PostProcess::PostProcess(std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept
	{

		// post process

		std::shared_ptr<DescriptorSetInfo> pp_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		pp_descriptor_set_create_info->AddBinding(DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_PIXEL_SHADER);
		m_pp_descriptorset = std::make_shared<DescriptorSet>(_device, pp_descriptor_set_create_info);
		std::shared_ptr<DescriptorSetLayouts> pp_descriptor_set_layout = std::make_shared<DescriptorSetLayouts>();
		pp_descriptor_set_layout->layouts.push_back(m_pp_descriptorset->GetLayout());

		PipelineCreateInfo pp_ipeline_create_info;
		pp_ipeline_create_info.name = "pp";
		pp_ipeline_create_info.vs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("postprocess.vert.spv"));
		pp_ipeline_create_info.ps = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("postprocess.frag.spv"));
		pp_ipeline_create_info.descriptor_layouts = pp_descriptor_set_layout;

		std::vector<AttachmentCreateInfo> pp_attachment_create_info{
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, _render_context.width, _render_context.height},
		};
		_pipeline_manager->createPipeline(pp_ipeline_create_info, pp_attachment_create_info, _render_context);
		m_pipeline = _pipeline_manager->Get("pp");
	}

	PostProcess::~PostProcess() noexcept
	{
	}

	void PostProcess::UpdateDescriptorSets() noexcept
	{
		m_pp_descriptorset->AllocateDescriptorSet();
		m_pp_descriptorset->UpdateDescriptorSet(m_descriptor_set_update_desc);
	}

	void PostProcess::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept
	{
		m_descriptor_set_update_desc.BindResource(binding, buffer);
	}

	std::shared_ptr<AttachmentDescriptor> PostProcess::GetFrameBufferAttachment(u32 _index) const noexcept
	{
		return m_pipeline->GetFrameBufferAttachment(_index);
	}

	std::shared_ptr<DescriptorSet> PostProcess::GetDescriptorSet() const noexcept
	{
		return m_pp_descriptorset;
	}

	std::shared_ptr<Pipeline> PostProcess::GetPipeline() const noexcept
	{
		return m_pipeline;
	}

}