#pragma once
#include "utils.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include "Device.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include "Framebuffers.h"
#include "VertexBuffer.h"
#include "Assets.h"
class CommandBuffer
{
public:
	CommandBuffer(Device* device,
		SwapChain* swapchain,
		Pipeline* pipeline,
		Framebuffers* framebuffers);
	~CommandBuffer();
	VkSemaphore getImageAvailableSemaphore()const;
	VkSemaphore getRenderFinishedSemaphore()const;
	VkCommandBuffer* get(u32 i);
	void draw();
	VkCommandPool getCommandpool()const;
	void beginCommandRecording(const Assest& assest);
private:
	void createCommandPool();
	void allocateCommandBuffers();
	void createSyncObjects();
	void createSemaphores();
	void createFences();
private:

	Device* mDevice = nullptr;
	SwapChain* mSwapChain;
	Pipeline* mPipeline = nullptr;
	Framebuffers* mFramebuffers = nullptr;
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
