#pragma once

#include <vector>
#include <unordered_map>

#include <vulkan/vulkan.hpp>

#include <runtime/core/utils/utils.h>
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include "Descriptors.h"
#include "RenderPass.h"
#include "FrameBuffer.h"

namespace Horizon {

	struct PushConstants {
		std::vector<VkPushConstantRange> pushConstantRanges;
	};

	struct PipelineCreateInfo {
		std::shared_ptr<Shader> vs, ps;
		std::shared_ptr<DescriptorSetLayouts> descriptor_layouts;
		std::shared_ptr<PushConstants> push_constants;
		std::shared_ptr<DescriptorSet> pipeline_descriptor_set;
		//VkPipelineVertexInputStateCreateInfo;
		//descriptorsetlayout
	};

	class Pipeline
	{
	public:
		Pipeline(std::shared_ptr<Device> device, const PipelineCreateInfo& createInfo, const std::vector<AttachmentCreateInfo>& attachment_create_info, RenderContext& render_context, std::shared_ptr<SwapChain> swap_chain = nullptr);
		~Pipeline();
		VkPipeline get()const;
		VkPipelineLayout getLayout() const;
		VkViewport getViewport() const;
		VkRenderPass getRenderPass() const;
		VkFramebuffer getFrameBuffer() const;
		VkFramebuffer getFrameBuffer(u32 index) const;
		std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 attahmentIndex);
		std::vector<VkImage> getPresentImages();
		std::vector<VkClearValue> getClearValues();
		bool hasPushConstants();
		bool hasPipelineDescriptorSet();
		std::shared_ptr<DescriptorSet> getPipelineDescriptorSet();
	private:
		void createPipelineLayout(const PipelineCreateInfo& createInfo);
		void createPipeline(const PipelineCreateInfo& createInfo);
	private:
		RenderContext& m_render_context;
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<Framebuffer> m_framebuffer = nullptr;
		std::shared_ptr<PushConstants> m_push_constants = nullptr;
		std::shared_ptr<DescriptorSet> m_pipeline_descriptor_set = nullptr;
		std::vector<VkClearValue> m_clear_values;
		// handle of pipeline layout
		VkPipelineLayout m_pipeline_layout = nullptr;
		VkPipeline m_pipeline;
		VkViewport m_viewport;

	};

	class PipelineManager {
	public:
		PipelineManager(std::shared_ptr<Device> device);
		void createPipeline(const PipelineCreateInfo& createInfo, const std::string& name, const std::vector<AttachmentCreateInfo>& attachment_create_info, RenderContext& render_context);
		void createPresentPipeline(const PipelineCreateInfo& createInfo, const std::vector<AttachmentCreateInfo>& attachment_create_info, RenderContext& render_context, std::shared_ptr<SwapChain> swap_chain);
		std::shared_ptr<Pipeline> get(const std::string& name);
	private:
		// convert pipelinecreateinfo and pipelinename to u32 hash key, https://dev.to/muiz6/string-hashing-in-c-1np3
		inline u32 GetPipelineKey(const std::string& key) {
			u32 hashCode = 0;
			for (u32 i = 0; i < key.length(); i++) {
				hashCode += key[i] * pow(16, i);
			}
			return hashCode;
		}
	private:
		struct PipelineVal {
			std::shared_ptr<Pipeline> pipeline;
		};
		std::shared_ptr<Device> m_device;
		std::shared_ptr<SwapChain> m_swap_chain;
		std::unordered_map<u32, PipelineVal> m_pipeline_map;
	};
}