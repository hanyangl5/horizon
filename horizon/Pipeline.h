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
	Pipeline(std::shared_ptr<Instance> instance,
		std::shared_ptr<Device> device,
		std::shared_ptr<Surface> surface,
		std::shared_ptr<SwapChain> swapchain);
	~Pipeline();

private:
	std::shared_ptr<Instance> mInstance;
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<Surface> mSurface;
	std::shared_ptr<SwapChain> mSwapChain;
	VkPipelineLayout mPipelineLayout;
};
