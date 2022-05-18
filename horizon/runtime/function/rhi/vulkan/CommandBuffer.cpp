#include "CommandBuffer.h"

#include <runtime/core/log/Log.h>
#include <runtime/function/rhi/vulkan/Texture.h>
#include <memory>

namespace Horizon {

	CommandBuffer::CommandBuffer(RenderContext& render_context, std::shared_ptr<Device> device) :m_render_context(render_context), m_device(device)
	{
		createCommandPool();
		allocateCommandBuffers();
		createSyncObjects();
	}

	CommandBuffer::~CommandBuffer()
	{
		for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(m_device->Get(), m_render_finished_semaphores[i], nullptr);
			vkDestroySemaphore(m_device->Get(), m_image_available_semaphores[i], nullptr);
			vkDestroyFence(m_device->Get(), m_in_flight_fences[i], nullptr);
		}
		vkDestroyCommandPool(m_device->Get(), m_command_pool, nullptr);
	}


	VkCommandBuffer CommandBuffer::Get(u32 i) const noexcept 
	{
		return m_command_buffers[i];
	}

	void CommandBuffer::submit(std::shared_ptr<SwapChain> swap_chain)
	{
		vkWaitForFences(m_device->Get(), 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		vkAcquireNextImageKHR(m_device->Get(), swap_chain->Get(), UINT64_MAX, m_image_available_semaphores[m_current_frame], VK_NULL_HANDLE, &imageIndex);

		if (m_images_in_flight[imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(m_device->Get(), 1, &m_images_in_flight[imageIndex], VK_TRUE, UINT64_MAX);
		}
		m_images_in_flight[imageIndex] = m_in_flight_fences[m_current_frame];

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { m_image_available_semaphores[m_current_frame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_command_buffers[imageIndex];

		VkSemaphore signalSemaphores[] = { m_render_finished_semaphores[m_current_frame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(m_device->Get(), 1, &m_in_flight_fences[m_current_frame]);

		CHECK_VK_RESULT(vkQueueSubmit(m_device->getGraphicQueue(), 1, &submitInfo, m_in_flight_fences[m_current_frame]));

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swap_chain->Get() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR(m_device->getPresnetQueue(), &presentInfo);

		m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
		vkQueueWaitIdle(m_device->getGraphicQueue());
	}

	VkCommandPool CommandBuffer::getCommandpool() const noexcept 
	{
		return m_command_pool;
	}

	void CommandBuffer::createCommandPool()
	{
		// Command buffers are executed by submitting them on one of the device queues
		// like the graphicsand presentation queues we retrieved.Each command pool can
		// only allocate command buffers that are submitted on a single type of queue
		// We're going to record commands for drawing, which is why we've chosen the
		// graphics queue family.
		VkCommandPoolCreateInfo command_pool_create_info{};
		command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_create_info.queueFamilyIndex = m_device->getQueueFamilyIndices().getGraphics();
		command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		CHECK_VK_RESULT(vkCreateCommandPool(m_device->Get(), &command_pool_create_info, nullptr, &m_command_pool));

	}

	void CommandBuffer::allocateCommandBuffers()
	{
		// We can now start allocating command buffers and recording drawing commands in them.
		// Because one of the drawing commands involves binding the right VkFramebuffer, 
		// we'll actually have to record a command buffer for every image in the swap chain
		// once again. To that end, create a list of VkCommandBuffer objects as a class member.
		// Command buffers will be automatically freed when their command pool is destroyed, 
		// so we don't need an explicit cleanup.
		m_command_buffers.resize(m_render_context.swap_chain_image_count);

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = m_command_pool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = static_cast<u32>(m_command_buffers.size());

		CHECK_VK_RESULT(vkAllocateCommandBuffers(m_device->Get(), &commandBufferAllocateInfo, m_command_buffers.data()));
	}

	void CommandBuffer::beginRenderPass(u32 index, std::shared_ptr<Pipeline> pipeline, bool is_present) const noexcept 
	{
		std::shared_ptr<GraphicsPipeline> _pipeline = std::static_pointer_cast<GraphicsPipeline>(pipeline);
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _pipeline->getRenderPass();
		if (is_present) {
			renderPassInfo.framebuffer = _pipeline->getFrameBuffer(index);
		}
		else {
			renderPassInfo.framebuffer = _pipeline->getFrameBuffer();
		}
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ static_cast<u32>(_pipeline->getViewport().width), static_cast<u32>(-_pipeline->getViewport().height)};


		auto& clearValues = _pipeline->getClearValues();
		renderPassInfo.clearValueCount = static_cast<u32>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(m_command_buffers[index], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(m_command_buffers[index], 0, 1, &_pipeline->getViewport());
	}

	void CommandBuffer::endRenderPass(u32 index) const noexcept 
	{
		vkCmdEndRenderPass(m_command_buffers[index]);
	}


	void CommandBuffer::createSyncObjects()
	{
		createSemaphores();
		createFences();
	}

	void CommandBuffer::createSemaphores()
	{
		m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			CHECK_VK_RESULT(vkCreateSemaphore(m_device->Get(), &semaphoreCreateInfo, nullptr, &m_image_available_semaphores[i]));
			CHECK_VK_RESULT(vkCreateSemaphore(m_device->Get(), &semaphoreCreateInfo, nullptr, &m_render_finished_semaphores[i]));
		}
	}

	void CommandBuffer::createFences()
	{

		m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
		m_images_in_flight.resize(m_render_context.swap_chain_image_count, VK_NULL_HANDLE);

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			CHECK_VK_RESULT(vkCreateFence(m_device->Get(), &fenceInfo, nullptr, &m_in_flight_fences[i]));
		}
	}

	VkCommandBuffer CommandBuffer::beginSingleTimeCommands() {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_command_pool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer command_buffer;
		vkAllocateCommandBuffers(m_device->Get(), &allocInfo, &command_buffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(command_buffer, &beginInfo);

		return command_buffer;
	}

	void CommandBuffer::endSingleTimeCommands(VkCommandBuffer command_buffer) {
		vkEndCommandBuffer(command_buffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &command_buffer;

		vkQueueSubmit(m_device->getGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_device->getGraphicQueue());

		vkFreeCommandBuffers(m_device->Get(), m_command_pool, 1, &command_buffer);
	}

	u32 CommandBuffer::present()
	{
		return 1;
	}

	void CommandBuffer::beginCommandRecording(u32 i)
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		// begin command buffer recording
		CHECK_VK_RESULT(vkBeginCommandBuffer(m_command_buffers[i], &commandBufferBeginInfo));
	}

	void CommandBuffer::endCommandRecording(u32 i)
	{
		CHECK_VK_RESULT(vkEndCommandBuffer(m_command_buffers[i]));
	}

	void CommandBuffer::Dispatch(u32 i, std::shared_ptr<Pipeline> pipeline, const std::vector<std::shared_ptr<DescriptorSet>> _descriptor_sets) noexcept
	{
		if (pipeline->GetType() != PipelineType::COMPUTE) {
			LOG_ERROR("incorrect pipeline type");
			return;
		}

		if (pipeline->hasPushConstants()) {
			for (auto& pc : pipeline->m_push_constants->ranges) {
				vkCmdPushConstants(m_command_buffers[i], pipeline->GetLayout(), ToVkShaderStageFlags(pc.stages), pc.offset, pc.size, pc.value);
			}
		}

		std::shared_ptr<ComputePipeline> _pipeline = std::static_pointer_cast<ComputePipeline>(pipeline);
		if (!_descriptor_sets.empty()) {
			std::vector<VkDescriptorSet> descriptor_sets(_descriptor_sets.size());
			for (u32 i = 0; i < _descriptor_sets.size(); i++)
			{
				descriptor_sets[i] = _descriptor_sets[i]->Get();
			}
			vkCmdBindDescriptorSets(m_command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline->GetLayout(), 0, descriptor_sets.size(), descriptor_sets.data(), 0, 0);
		}
		vkCmdBindPipeline(m_command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline->Get());
		vkCmdDispatch(m_command_buffers[i], _pipeline->GroupCountX(), _pipeline->GroupCountY(), _pipeline->GroupCountZ());
	}
}