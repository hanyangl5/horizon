#pragma once

#include <vector>
#include <unordered_map>

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include "Descriptors.h"
#include "RenderPass.h"

namespace Horizon {

	class Pipeline
	{
	public:
		Pipeline(Device* device, SwapChain* swapchain, RenderPass* renderpass);
		~Pipeline();
		void destroy();
		void create(std::vector<DescriptorSet>& descriptors);
		VkPipeline get()const;
		VkPipelineLayout getLayout() const;
		VkViewport getViewport() const;
		VkRenderPass getRenderPass() const;
	private:
		void createPipelineLayout(std::vector<DescriptorSet>& descriptors);
		void createPipeline();
	private:
		Device* mDevice = nullptr;
		SwapChain* mSwapChain = nullptr;
		//DescriptorSet* mDescriptorSet = nullptr;
		RenderPass* mRenderPass = nullptr;
		// handle of pipeline layout
		VkPipelineLayout mPipelineLayout = nullptr;
		VkPipeline mGraphicsPipeline;
		VkViewport mViewport;

	};

	class PipelinaManager {
		struct PipelineCreateInfo {
			VkShaderModule vs, ps;
			//VkPipelineVertexInputStateCreateInfo;
		};
		void create(PipelineCreateInfo info);

		struct PipelineKey {
			// descriptor
			// vs, ps
		};
		struct PipelineVal {
			Pipeline pipeline;
		};
		std::unordered_map<PipelineKey, PipelineVal> mPipelineMap;
	};
}