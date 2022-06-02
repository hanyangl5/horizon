#pragma once

#include <string>
#include <vector>

#include "vk_mem_alloc.h"

#include <runtime/core/utils/definations.h>
#include <runtime/core/log/Log.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer2.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>

namespace Horizon
{
	namespace RHI
	{
		class RHIVulkan : public RHIInterface
		{
		public:
			RHIVulkan() noexcept;
			virtual ~RHIVulkan() noexcept;

			virtual void InitializeRenderer() noexcept override;

			virtual Buffer *CreateBuffer(const BufferCreateInfo &buffer_create_info) noexcept override;
			virtual void DestroyBuffer(Buffer *buffer) noexcept override;

			virtual Texture2 *CreateTexture(const TextureCreateInfo &texture_create_info) noexcept override;
			virtual void DestroyTexture(Texture2 *texture) noexcept override;

			virtual void CreateSwapChain(std::shared_ptr<Window> window) noexcept override;
			
			virtual ShaderProgram CreateShaderProgram(
				ShaderTargetStage stage,
				const std::string& entry_point,
				u32 compile_flags,
				std::string file_name) noexcept override;
		private:
			void InitializeVulkanRenderer(const std::string &app_name) noexcept;
			void CreateInstance(const std::string &app_name,
								std::vector<const char *> &instance_layers,
								std::vector<const char *> &instance_extensions) noexcept;
			void PickGPU(VkInstance instance, VkPhysicalDevice *gpu) noexcept;
			void CreateDevice(std::vector<const char *> &device_extensions) noexcept;
			void InitializeVMA() noexcept;

		private:
			struct VulkanRendererContext
			{
				VkInstance instance;
				VkPhysicalDevice active_gpu;
				// VkPhysicalDeviceProperties* vk_active_gpu_properties;
				VkDevice device;
				VmaAllocator vma_allocator;
				VkQueue graphics_queue, compute_queue, transfer_queue;
				u32 graphics_queue_family_index, compute_queue_family_index, transfer_queue_family_index;
				VkSurfaceKHR surface;
				VkSwapchainKHR swap_chain;
				std::vector<VkImage> swap_chain_images;
				std::vector<VkImageView> swap_chain_image_views;
			} m_vulkan;
		};
	}

}