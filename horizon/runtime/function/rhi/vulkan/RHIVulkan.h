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

namespace Horizon {
	namespace RHI {
		class RHIVulkan : public RHIInterface
		{
		public:
			RHIVulkan();
			virtual ~RHIVulkan();

			virtual void InitializeRenderer() override;

			virtual Buffer* CreateBuffer(const BufferCreateInfo& buffer_create_info) override;
			virtual void DestroyBuffer(Buffer* buffer) override;

			virtual Texture2* CreateTexture(const TextureCreateInfo& texture_create_info) override;
			virtual void DestroyTexture(Texture2* texture) override;

			virtual void CreateSwapChain(std::shared_ptr<Window> window) override;
		private:
			void InitializeVulkanRenderer(const std::string& app_name);
			void CreateInstance(const std::string& app_name,
				std::vector<const char*>& instance_layers,
				std::vector<const char*>& instance_extensions);
			void PickGPU(VkInstance instance, VkPhysicalDevice* gpu);
			void CreateDevice(std::vector<const char*>& device_extensions);
			void InitializeVMA();

		private:
			struct VulkanRendererContext {
				VkInstance                   instance;
				VkPhysicalDevice             active_gpu;
				//VkPhysicalDeviceProperties* vk_active_gpu_properties;
				VkDevice                     device;
				VmaAllocator vma_allocator;
				VkQueue graphics_queue, compute_queue, transfer_queue;
				u32 graphics_queue_family_index, compute_queue_family_index, transfer_queue_family_index;
				VkSurfaceKHR surface;
				VkSwapchainKHR swap_chain;
				std::vector<VkImage> swap_chain_images;
				std::vector<VkImageView> swap_chain_image_views;
			}m_vulkan;
		};
	}

}