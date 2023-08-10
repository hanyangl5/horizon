#pragma once

#include <vector>
#include <unordered_map>

#include <vulkan/vulkan.hpp>

#include <runtime/function/rhi/RenderContext.h>
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include "Descriptors.h"
#include "RenderPass.h"
#include "Framebuffer.h"
namespace Horizon
{

	struct GraphicsPipelineCreateInfo
	{
		std::string name;
		std::shared_ptr<Shader> vs, ps;
		std::shared_ptr<DescriptorSetLayouts> descriptor_layouts;
		std::shared_ptr<PushConstants> push_constants;
		// VkPipelineVertexInputStateCreateInfo;
		// descriptorsetlayout
	};

	struct ComputePipelineCreateInfo {
		std::string name;
		std::shared_ptr<Shader> cs;
		std::shared_ptr<DescriptorSetLayouts> descriptor_layouts;
		std::shared_ptr<PushConstants> push_constants;
		u32 group_count_x = 1, group_count_y = 1, group_count_z = 1;
	};

	class Pipeline
	{
	public:
		Pipeline(std::shared_ptr<Device> device) noexcept;
		~Pipeline() noexcept;
		VkPipeline Get() const noexcept;
		VkPipelineLayout GetLayout() const noexcept;
		bool hasPushConstants() const noexcept;
		PipelineType GetType() const noexcept;
	public:
		std::shared_ptr<PushConstants> m_push_constants = nullptr;
	protected:
		PipelineType m_type;
		std::shared_ptr<Device> m_device = nullptr;
		VkPipelineLayout m_pipeline_layout = nullptr;
		VkPipeline m_pipeline;
	};

	class GraphicsPipeline : public Pipeline {
	public:
		GraphicsPipeline(std::shared_ptr<Device> device,
			const GraphicsPipelineCreateInfo& create_info,
			const std::vector<AttachmentCreateInfo>& attachment_create_info,
			const RenderContext render_context,
			std::shared_ptr<SwapChain> swap_chain = nullptr) noexcept;
		~GraphicsPipeline() noexcept;

		VkViewport getViewport() const noexcept;
		VkRenderPass getRenderPass() const noexcept;
		VkFramebuffer getFrameBuffer() const noexcept;
		VkFramebuffer getFrameBuffer(u32 index) const noexcept;
		std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 attahmentIndex) const noexcept;
		std::vector<VkImage> getPresentImages() const noexcept;
		std::vector<VkClearValue> getClearValues() const noexcept;

	private:
		void CreatePipelineLayout(const GraphicsPipelineCreateInfo& create_info);
		void CreatePipeline(const GraphicsPipelineCreateInfo& create_info);
	private:
		RenderContext m_render_context;		   
		std::shared_ptr<Framebuffer> m_framebuffer = nullptr;
		std::vector<VkClearValue> m_clear_values;
		VkViewport m_viewport;
	};

	class ComputePipeline : public Pipeline {
	public:
		ComputePipeline(std::shared_ptr<Device> device, const ComputePipelineCreateInfo& create_info) noexcept;
		~ComputePipeline() noexcept;
		u32 GroupCountX() const noexcept;
		u32 GroupCountY() const noexcept;
		u32 GroupCountZ() const noexcept;
	private:
		void CreatePipelineLayout(const ComputePipelineCreateInfo& create_info) noexcept;
		void CreatePipeline(const ComputePipelineCreateInfo& create_info) noexcept;
	private:
		u32 m_group_count_x, m_group_count_y, m_group_count_z;
	};

	class RTpipeline : public Pipeline{
		public:
	};

	class PipelineManager
	{
	public:
		PipelineManager(std::shared_ptr<Device> device);

		std::shared_ptr<Pipeline> CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info,
			const std::vector<AttachmentCreateInfo> &_attachment_create_info,
			const RenderContext _render_context);

		std::shared_ptr<Pipeline> CreateComputePipeline(const ComputePipelineCreateInfo& create_info);

		void createPresentPipeline(const GraphicsPipelineCreateInfo &create_info,
			const std::vector<AttachmentCreateInfo> &_attachment_create_info,
			RenderContext& _render_context,
			std::shared_ptr<SwapChain> swap_chain);

		std::shared_ptr<Pipeline> Get(const std::string &name);

	private:
		// convert pipelinecreateinfo and pipelinename to u32 hash key, https://dev.to/muiz6/string-hashing-in-c-1np3
		inline std::string GetPipelineKey(const GraphicsPipelineCreateInfo& create_info)
		{
			return create_info.name;
		}

		inline std::string GetPipelineKey(const ComputePipelineCreateInfo& create_info)
		{
			return create_info.name;
		}

	private:
		struct PipelineVal
		{
			std::shared_ptr<Pipeline> pipeline;
		};
		std::shared_ptr<Device> m_device;
		std::shared_ptr<SwapChain> m_swap_chain;
		std::unordered_map<std::string, PipelineVal> m_pipeline_map;
	};
}