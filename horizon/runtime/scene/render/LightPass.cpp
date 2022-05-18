#include "LightPass.h"
#include <runtime/function/rhi/vulkan/VulkanEnums.h>
#include <runtime/function/rhi/RenderContext.h>
#include <runtime/core/path/Path.h>

namespace Horizon
{
	LightPass::LightPass(std::shared_ptr<Scene> _scene, std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext& _render_context) noexcept : m_device(_device)
	{
		CreateResources();
		GraphicsPipelineCreateInfo LightPassPipelineCreateInfo;
		LightPassPipelineCreateInfo.name = "LightPass";
		LightPassPipelineCreateInfo.vs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("simplevs.vert.spv"));
		LightPassPipelineCreateInfo.ps = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("shading.frag.spv"));
		LightPassPipelineCreateInfo.descriptor_layouts = m_descriptor_set_layout;


		std::vector<AttachmentCreateInfo> LightPassAttachmentsCreateInfo{
			AttachmentCreateInfo{TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT, COLOR_ATTACHMENT, TextureType::TEXTURE_TYPE_2D, _render_context.width, _render_context.height, 1}
		};

		m_pipeline = _pipeline_manager->CreateGraphicsPipeline(LightPassPipelineCreateInfo, LightPassAttachmentsCreateInfo, _render_context);
	}

	LightPass::~LightPass() noexcept
	{
	}

	void LightPass::CreateResources() noexcept
	{
		std::shared_ptr<DescriptorSetInfo> descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
		descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER);
		descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER);
		descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER);
		descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER);
		descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER);
		descriptor_set_create_info->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_PIXEL_SHADER);

		m_descriptorset = std::make_shared<DescriptorSet>(m_device, descriptor_set_create_info);

		m_descriptor_set_layout = std::make_shared<DescriptorSetLayouts>();
		m_descriptor_set_layout->layouts.push_back(m_descriptorset->GetLayout());

	}

	void LightPass::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept
	{
		m_descriptor_set_update_desc.BindResource(binding, buffer);
	}
	void LightPass::UpdateDescriptorSets() noexcept
	{
		m_descriptorset->UpdateDescriptorSet(m_descriptor_set_update_desc);
	}
	std::shared_ptr<AttachmentDescriptor> LightPass::GetFrameBufferAttachment(u32 _index) const noexcept
	{
		return std::static_pointer_cast<GraphicsPipeline>(m_pipeline)->GetFrameBufferAttachment(_index);
	}

	std::shared_ptr<Pipeline> LightPass::GetPipeline() const noexcept
	{
		return m_pipeline;
	}

}