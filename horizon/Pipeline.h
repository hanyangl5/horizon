#pragma once
#include "utils.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"

class Pipeline
{
public:
	Pipeline(std::shared_ptr<Device> device,
		std::shared_ptr<SwapChain> swapchain);
	~Pipeline();
	VkPipeline get()const;
	VkRenderPass getRenderPass()const;
	VkViewport getViewport()const;
private:
	void createPipelineLayout();
	void createRenderPass();
	void createPipeline();
private:
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<SwapChain> mSwapChain;
	// handle of pipeline layout
	VkPipelineLayout mPipelineLayout;
	VkRenderPass mRenderPass;
	VkPipeline mGraphicsPipeline;
	VkViewport mViewport;

};
