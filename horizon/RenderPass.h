#pragma once
#include <vulkan/vulkan.hpp>
#include "Device.h"
#include "SwapChain.h"
#include "utils.h"
class RenderPass
{
public:
	RenderPass(Device* device, SwapChain* swapChain);
	~RenderPass();
	VkRenderPass get() const;
private:
	void createRenderPass();
private:
	Device* mDevice = nullptr;
	SwapChain* mSwapChain = nullptr;
	VkRenderPass mRenderPass;
};

