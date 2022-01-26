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
	Pipeline(Device* device, SwapChain* swapchain, Descriptors* descriptors);
	~Pipeline();
	VkPipeline get()const;
	VkPipelineLayout getLayout()const;
	VkRenderPass getRenderPass()const;
	VkViewport getViewport()const;
	VkDescriptorSetLayout* getDescriptorSetLayouts();
	u32 getDescriptorCount();
	VkDescriptorSet* getDescriptorSets();
private:
	void createPipelineLayout(Descriptors* descriptors);
	void createRenderPass();
	void createPipeline();
private:
	Device* mDevice = nullptr;
	SwapChain* mSwapChain = nullptr;
	Descriptors* mDescriptors = nullptr;
	// handle of pipeline layout
	VkPipelineLayout mPipelineLayout = nullptr;
	VkRenderPass mRenderPass;
	VkPipeline mGraphicsPipeline;
	VkViewport mViewport;

};
