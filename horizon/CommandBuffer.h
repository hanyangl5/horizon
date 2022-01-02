#pragma once
#include "utils.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include "Device.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include "Framebuffers.h"
class CommandBuffer
{
public:
	CommandBuffer(std::shared_ptr<Device> device,
		std::shared_ptr<SwapChain> swapchain,
		std::shared_ptr<Pipeline> pipeline,
		std::shared_ptr<Framebuffers> framebuffers);
	~CommandBuffer();

private:
	void createCommandPool();
	void allocateCommandBuffers();
	void beginCommandRecording();

private:

	std::shared_ptr<Device> mDevice;
	std::shared_ptr<SwapChain> mSwapChain;
	std::shared_ptr<Pipeline> mPipeline;
	std::shared_ptr<Framebuffers> mFramebuffers;
	VkCommandPool mCommandPool;
	std::vector<VkCommandBuffer> mCommandBuffers;
};
