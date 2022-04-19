#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include <runtime/function/render/RenderContext.h>
#include "Device.h"
#include "SwapChain.h"
#include "Pipeline.h"

namespace Horizon {

	class CommandBuffer
	{
	public:
		CommandBuffer(RenderContext& render_context, std::shared_ptr<Device> device);
		~CommandBuffer();
		VkCommandBuffer Get(u32 i);
		void submit(std::shared_ptr<SwapChain> swap_chain);
		VkCommandPool getCommandpool()const;
		void beginRenderPass(u32 index, std::shared_ptr<Pipeline> pipeline, bool is_present = false);
		void endRenderPass(u32 index);
		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer command_buffer);
		u32 commandBufferCount() const noexcept { return m_command_buffers.size(); }
		u32 present();
		void beginCommandRecording(u32 index);
		void endCommandRecording(u32 index);

	private:
		void createCommandPool();
		void allocateCommandBuffers();
		void createSyncObjects();
		void createSemaphores();
		void createFences();
	private:
		RenderContext& m_render_context;
		std::shared_ptr<Device> m_device = nullptr;

		VkCommandPool m_command_pool = nullptr;
		std::vector<VkCommandBuffer> m_command_buffers;

		// We'll need one semaphore to signal that an image has been acquired and is ready for rendering,
		// and another one to signal that rendering has finished and presentation can happen. Create two 
		// class members to store these semaphore objects:
		std::vector<VkSemaphore> m_image_available_semaphores;
		std::vector<VkSemaphore> m_render_finished_semaphores;
		std::vector<VkFence> m_in_flight_fences;
		std::vector<VkFence> m_images_in_flight;
		const int MAX_FRAMES_IN_FLIGHT = 2;
		u32 m_current_frame = 0;
	};

}