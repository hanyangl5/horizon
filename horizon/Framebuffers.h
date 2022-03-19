#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "Device.h"
#include "SwapChain.h"
#include "RenderPass.h"

namespace Horizon {
	//we have to create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time.
	class Framebuffers
	{
	public:
		Framebuffers(Device* device, SwapChain* swapchain, RenderPass* renderPass);
		~Framebuffers();
		VkFramebuffer get(u32 i)const;
	private:
		void createFrameBuffers();
	private:

		SwapChain* mSwapChain;
		Device* mDevice = nullptr;
		RenderPass* mRenderPass;
		std::vector<VkFramebuffer> mSwapChainFrameBuffers;
	};
}
