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
		CommandBuffer(std::shared_ptr<Device> device,
			std::shared_ptr<SwapChain> swapchain);
		~CommandBuffer();
		VkCommandBuffer* get(u32 i);
		void submit();
		VkCommandPool getCommandpool()const;
		void beginRenderPass(u32 index, std::shared_ptr<Pipeline> pipeline);
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

		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<SwapChain> mSwapChain;

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