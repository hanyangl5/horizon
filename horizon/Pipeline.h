#pragma once
#include "utils.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include "Descriptors.h"

class Pipeline
{
public:
	Pipeline(std::shared_ptr<Device> device,
		std::shared_ptr<SwapChain> swapchain,
		std::shared_ptr<Descriptors> descriptors);
	~Pipeline();
	VkPipeline get()const;
	VkPipelineLayout getLayout()const;
	VkRenderPass getRenderPass()const;
	VkViewport getViewport()const;
	VkDescriptorSetLayout* getDescriptorSetLayouts();
	u32 getDescriptorCount();
	VkDescriptorSet* getDescriptorSets();
private:
	void createPipelineLayout(std::shared_ptr<Descriptors> descriptors);
	void createRenderPass();
	void createPipeline();
private:
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<SwapChain> mSwapChain;
	std::shared_ptr<Descriptors> mDescriptors;
	// handle of pipeline layout
	VkPipelineLayout mPipelineLayout;
	VkRenderPass mRenderPass;
	VkPipeline mGraphicsPipeline;
	VkViewport mViewport;

};
