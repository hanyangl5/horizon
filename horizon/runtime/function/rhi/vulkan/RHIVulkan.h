#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>

#include "vk_mem_alloc.h"

#include <runtime/core/utils/Definations.h>
#include <runtime/core/log/Log.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptors.h>

namespace Horizon::RHI {

	class RHIVulkan : public RHIInterface
	{
	public:
		RHIVulkan() noexcept;
		virtual ~RHIVulkan() noexcept;

		virtual void InitializeRenderer() noexcept override;

		virtual Buffer* CreateBuffer(const BufferCreateInfo& buffer_create_info) noexcept override;
		virtual void DestroyBuffer(Buffer* buffer) noexcept override;

		virtual Texture* CreateTexture(const TextureCreateInfo& texture_create_info) noexcept override;
		virtual void DestroyTexture(Texture* texture) noexcept override;

		virtual void CreateSwapChain(std::shared_ptr<Window> window) noexcept override;

		virtual ShaderProgram* CreateShaderProgram(
			ShaderType type,
			const std::string& entry_point,
			u32 compile_flags,
			std::string file_name) noexcept override;

		virtual void DestroyShaderProgram(ShaderProgram* shader_program) noexcept override;

		virtual CommandList* GetCommandList(CommandQueueType type) noexcept override;
		virtual void ResetCommandResources() noexcept override;

		virtual Pipeline* CreatePipeline(const PipelineCreateInfo& pipeline_create_info) noexcept override;
	private:
		void InitializeVulkanRenderer(const std::string& app_name) noexcept;
		void CreateInstance(const std::string& app_name,
			std::vector<const char*>& instance_layers,
			std::vector<const char*>& instance_extensions) noexcept;
		void PickGPU(VkInstance instance, VkPhysicalDevice* gpu) noexcept;
		void CreateDevice(std::vector<const char*>& device_extensions) noexcept;
		void InitializeVMA() noexcept;

		void DestroySwapChain() noexcept;

		// submit command list to command queue
		virtual void SubmitCommandLists(CommandQueueType queue_type, std::vector<CommandList*>& command_lists)  noexcept override;

	private:

	private:
		struct VulkanRendererContext
		{
			VkInstance instance;
			VkPhysicalDevice active_gpu;
			// VkPhysicalDeviceProperties* vk_active_gpu_properties;
			VkDevice device;
			VmaAllocator vma_allocator;
			std::array<VkQueue, 3> command_queues;
			VkSurfaceKHR surface;
			VkSwapchainKHR swap_chain;
			std::vector<VkImage> swap_chain_images;
			std::vector<VkImageView> swap_chain_image_views;
		} m_vulkan{};
		VulkanDescriptor* m_global_descriptor = nullptr;
		// pipeline map
		// resource manager, auto 
	};
}

