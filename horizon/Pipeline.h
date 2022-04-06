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
#include "FrameBuffer.h"

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
		Pipeline(std::shared_ptr<Device> device, const PipelineCreateInfo& createInfo, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext, std::shared_ptr<SwapChain> swapChain = nullptr);
		~Pipeline();
		VkPipeline get()const;
		VkPipelineLayout getLayout() const;
		VkViewport getViewport() const;
		VkRenderPass getRenderPass() const;
		VkFramebuffer getFrameBuffer() const;
		VkFramebuffer getFrameBuffer(u32 index) const;
		std::shared_ptr<AttachmentDescriptor> getFramebufferDescriptorImageInfo(u32 attahmentIndex);
		void attachToSwapChain(std::shared_ptr<SwapChain> swapChain);
		std::vector<VkImage> getPresentImages();
	private:
		void createPipelineLayout(const PipelineCreateInfo& createInfo);
		void createPipeline(const PipelineCreateInfo& createInfo);
	private:
		RenderContext& mRenderContext;
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<Framebuffer> mFramebuffer = nullptr;

		// handle of pipeline layout
		VkPipelineLayout mPipelineLayout = nullptr;
		VkPipeline mGraphicsPipeline;
		VkViewport mViewport;

	};

	class PipelineManager {
	public:
		PipelineManager(std::shared_ptr<Device> device);
		void createPipeline(const PipelineCreateInfo& createInfo, const std::string& name, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext);
		void createPresentPipeline(const PipelineCreateInfo& createInfo, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext, std::shared_ptr<SwapChain> swapChain);
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