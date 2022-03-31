#pragma once

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "SwapChain.h"

namespace Horizon {

	class RenderPass
	{
	public:
		RenderPass(std::shared_ptr<Device> device, std::shared_ptr<SwapChain> swapChain);
		~RenderPass();
		VkRenderPass get() const;
	private:
		void createRenderPass();
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<SwapChain> mSwapChain = nullptr;
		VkRenderPass mRenderPass;
	};
}
