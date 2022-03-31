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
#include "FrameBuffers.h"

namespace Horizon {


	struct PipelineCreateInfo {
		std::shared_ptr<Shader> vs, ps;
		std::shared_ptr<DescriptorSetLayouts> descriptorLayouts;
		//VkPipelineVertexInputStateCreateInfo;
		//descriptorsetlayout
	};

	class Pipeline
	{
	public:
		Pipeline(std::shared_ptr<Device> device, std::shared_ptr<SwapChain> swapchain, std::shared_ptr<PipelineCreateInfo> createInfo);
		~Pipeline();
		void destroy();
		void create();
		VkPipeline get()const;
		VkPipelineLayout getLayout() const;
		VkViewport getViewport() const;
		VkRenderPass getRenderPass() const;
		VkFramebuffer getFrameBuffer(u32 index) const;
	private:
		void createPipelineLayout();
		void createPipeline();
	private:

		std::shared_ptr<PipelineCreateInfo> mCreateInfo;

		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<SwapChain> mSwapChain = nullptr;
		std::shared_ptr<RenderPass> mRenderPass = nullptr;
		std::shared_ptr<Framebuffers> mFramebuffers = nullptr;

		// handle of pipeline layout
		VkPipelineLayout mPipelineLayout = nullptr;
		VkPipeline mGraphicsPipeline;
		VkViewport mViewport;

	};

	class PipelineManager {
	public:
		PipelineManager(std::shared_ptr<Device> device, std::shared_ptr<SwapChain> swapChain);
		void createPipeline(std::shared_ptr<PipelineCreateInfo> createInfo, const std::string& name);
		std::shared_ptr<Pipeline> get(const std::string& name);
	private:
		// convert pipelinecreateinfo and pipelinename to u32 hash key, https://dev.to/muiz6/string-hashing-in-c-1np3
		inline u32 pipelineHash(const std::string& key) {
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
		std::shared_ptr<Device> mDevice;
		std::shared_ptr<SwapChain> mSwapChain;
		std::unordered_map<u32, PipelineVal> mPipelineMap;
	};
}