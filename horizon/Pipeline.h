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
		Shader* vs, * ps;
		DescriptorSetLayouts descriptorLayouts;
		//VkPipelineVertexInputStateCreateInfo;
		//descriptorsetlayout
	};

	class Pipeline
	{
	public:
		Pipeline(Device* device, SwapChain* swapchain, PipelineCreateInfo* info);
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

		PipelineCreateInfo* mCreateInfo = nullptr;

		Device* mDevice = nullptr;
		SwapChain* mSwapChain = nullptr;
		RenderPass* mRenderPass = nullptr;
		Framebuffers* mFramebuffers = nullptr;
		
		// handle of pipeline layout
		VkPipelineLayout mPipelineLayout = nullptr;
		VkPipeline mGraphicsPipeline;
		VkViewport mViewport;

	};

	class PipelinaManager {
	public:

		//struct PipelineKey {
		//	// descriptor
		//	// vs, ps
		//};
		struct PipelineVal {
			Pipeline* pipeline;
		};
		Device* mDevice;
		SwapChain* mSwapChain;
		std::unordered_map<u32, PipelineVal> mPipelineMap;
	public:
		void init(Device* device, SwapChain* swapChain);
		void createPipeline(PipelineCreateInfo* info, const std::string& name);
		Pipeline* get(const std::string& name);
	private:
		// convert string to u32 hash key, https://dev.to/muiz6/string-hashing-in-c-1np3
		inline u32 stringHash(const std::string& key) {
			u32 hashCode = 0;
			for (u32 i = 0; i < key.length(); i++) {
				hashCode += key[i] * pow(16, i);
			}
			return hashCode;
		}
	};
}