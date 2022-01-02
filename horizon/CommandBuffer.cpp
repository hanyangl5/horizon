#include "CommandBuffer.h"


CommandBuffer::CommandBuffer(std::shared_ptr<Device> device,
	std::shared_ptr<SwapChain> swapchain,
	std::shared_ptr<Pipeline> pipeline,
	std::shared_ptr<Framebuffers>  framebuffers)
	:mDevice(device), mSwapChain(swapchain),mPipeline(pipeline),mFramebuffers(framebuffers)
{
	createCommandPool();
	allocateCommandBuffers();
	beginCommandRecording();
}

CommandBuffer::~CommandBuffer()
{
	vkDestroyCommandPool(mDevice->get(), mCommandPool, nullptr);
}

void CommandBuffer::createCommandPool()
{
	// Command buffers are executed by submitting them on one of the device queues
	// like the graphicsand presentation queues we retrieved.Each command pool can
	// only allocate command buffers that are submitted on a single type of queue
	// We're going to record commands for drawing, which is why we've chosen the
	// graphics queue family.
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = mDevice->getQueueFamilyIndices().getGraphics();
	commandPoolCreateInfo.flags = 0;

	printVkError(vkCreateCommandPool(mDevice->get(), &commandPoolCreateInfo, nullptr, &mCommandPool), "create command pool");

}

void CommandBuffer::allocateCommandBuffers()
{
	// We can now start allocating command buffers and recording drawing commands in them.
	// Because one of the drawing commands involves binding the right VkFramebuffer, 
	// we'll actually have to record a command buffer for every image in the swap chain
	// once again. To that end, create a list of VkCommandBuffer objects as a class member.
	// Command buffers will be automatically freed when their command pool is destroyed, 
	// so we don't need an explicit cleanup.
	mCommandBuffers.resize(mSwapChain->getImageCount());

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = mCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = static_cast<u32>(mCommandBuffers.size());

	printVkError(vkAllocateCommandBuffers(mDevice->get(), &commandBufferAllocateInfo, mCommandBuffers.data()), "allocate command buffers");
}

void CommandBuffer::beginCommandRecording()
{
	for (u32 i = 0; i < mCommandBuffers.size(); i++) {
		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		printVkError(vkBeginCommandBuffer(mCommandBuffers[i], &commandBufferBeginInfo),"begin command buffer recording");
	
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = mPipeline->getRenderPass();
		renderPassInfo.framebuffer = mFramebuffers->get(i);
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = mSwapChain->getExtent();

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(mCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->get());
		

		vkCmdSetViewport(mCommandBuffers[i], 0, 1,&mPipeline->getViewport());

		vkCmdDraw(mCommandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(mCommandBuffers[i]);

		printVkError(vkEndCommandBuffer(mCommandBuffers[i]), "end command buffer");
	}
}
