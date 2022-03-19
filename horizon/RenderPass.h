#pragma once

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "SwapChain.h"

namespace Horizon {

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
}
