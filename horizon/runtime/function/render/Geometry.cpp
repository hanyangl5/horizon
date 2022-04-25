#include "Geometry.h"
#include <runtime/function/render/rhi/VulkanEnums.h>
#include <runtime/function/render/RenderContext.h>
#include <runtime/core/path/Path.h>

namespace Horizon
{
	Geometry::Geometry(std::shared_ptr<Scene> _scene, std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept
	{

		PipelineCreateInfo geometryPipelineCreateInfo;
		geometryPipelineCreateInfo.name = "geometry";
		geometryPipelineCreateInfo.vs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("geometry.vert.spv"));
		geometryPipelineCreateInfo.ps = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("geometry.frag.spv"));
		geometryPipelineCreateInfo.descriptor_layouts = _scene->GetGeometryPassDescriptorLayouts();

		std::shared_ptr<PushConstants> geometryPipelinePushConstants = std::make_shared<PushConstants>();

		geometryPipelinePushConstants->pushConstantRanges = {{SHADER_STAGE_VERTEX_SHADER, 0, 2 * sizeof(Math::mat4)}}; // Push constants have a minimum size of 128 bytes
		geometryPipelineCreateInfo.push_constants = geometryPipelinePushConstants;
		// position + depth
		// normal
		// albedo
		// depth

		std::vector<AttachmentCreateInfo> geometryAttachmentsCreateInfo{
			{VK_FORMAT_R32G32B32A32_SFLOAT, COLOR_ATTACHMENT, _render_context.width, _render_context.height},
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, _render_context.width, _render_context.height},
			{VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, _render_context.width, _render_context.height},
			{VK_FORMAT_D32_SFLOAT, DEPTH_STENCIL_ATTACHMENT, _render_context.width, _render_context.height}};

		_pipeline_manager->createPipeline(geometryPipelineCreateInfo, geometryAttachmentsCreateInfo, _render_context);
		m_pipeline = _pipeline_manager->Get("geometry");
	}

	Geometry::~Geometry() noexcept
	{
	}

	void Geometry::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept
	{
	}

	std::shared_ptr<AttachmentDescriptor> Geometry::GetFrameBufferAttachment(u32 _index) const noexcept
	{
		return m_pipeline->GetFrameBufferAttachment(_index);
	}

	std::shared_ptr<Pipeline> Geometry::GetPipeline() const noexcept
	{
		return m_pipeline;
	}

}