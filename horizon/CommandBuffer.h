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
	CommandBuffer(std::shared_ptr<Device> device,
		std::shared_ptr<SwapChain> swapchain,
		std::shared_ptr<Pipeline> pipeline,
		std::shared_ptr<Framebuffers> framebuffers);
	~CommandBuffer();
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

	std::shared_ptr<Device> mDevice;
	std::shared_ptr<SwapChain> mSwapChain;
	std::shared_ptr<Pipeline> mPipeline;
	std::shared_ptr<Framebuffers> mFramebuffers;
	VkCommandPool mCommandPool;
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
