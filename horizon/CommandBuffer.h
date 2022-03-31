#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include "Framebuffers.h"

namespace Horizon {

	class CommandBuffer
	{
	public:
		CommandBuffer(Device* device,
			SwapChain* swapchain);
		~CommandBuffer();
		VkCommandBuffer* get(u32 i);
		void submit();
		VkCommandPool getCommandpool()const;
		void beginRenderPass(u32 index, Pipeline* pipeline);
		void endRenderPass(u32 index);
		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandbuffer);
		u32 commandBufferCount() const noexcept { return mCommandBuffers.size(); }
	private:
		void createCommandPool();
		void allocateCommandBuffers();
		void createSyncObjects();
		void createSemaphores();
		void createFences();
	private:

		Device* mDevice = nullptr;
		SwapChain* mSwapChain;

		VkCommandPool mCommandPool = nullptr;
		std::vector<VkCommandBuffer> mCommandBuffers;

		// We'll need one semaphore to signal that an image has been acquired and is ready for rendering,
		// and another one to signal that rendering has finished and presentation can happen. Create two 
		// class members to store these semaphore objects:
		std::vector<VkSemaphore> mImageAvailableSemaphores;
		std::vector<VkSemaphore> mRenderFinishedSemaphores;
		std::vector<VkFence> mInFlightFences;
		std::vector<VkFence> mImagesInFlight;
		const int MAX_FRAMES_IN_FLIGHT = 2;
		u32 currentFrame = 0;
	};

}