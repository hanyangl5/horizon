#pragma once

#include <vulkan/vulkan.hpp>

#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/window/Window.h>

#include "QueueFamilyIndices.h"
#include "ValidationLayer.h"

namespace Horizon {
	
	class Instance
	{
	public:
		Instance();
		~Instance();
		VkInstance Get()const noexcept;
		const ValidationLayer& getValidationLayer()const noexcept;
	private:
		void createInstance();
	private:
		VkInstance m_instance;
		u32 m_extension_count = 0;
		std::vector<VkExtensionProperties> m_extensions;
		ValidationLayer m_validation_layer;
	};

	class Surface
	{
	public:
		Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window);
		~Surface();
		VkSurfaceKHR Get()const noexcept;
	private:
		void createSurface();
	private:
		std::shared_ptr<Instance> m_instance = nullptr;
		std::shared_ptr<Window> m_window = nullptr;
		VkSurfaceKHR m_surface;
	};

	class Device
	{
	public:
		Device(std::shared_ptr<Window> window) noexcept;
		~Device() noexcept;
		VkPhysicalDevice getPhysicalDevice() const noexcept;
		VkDevice Get()const noexcept;
		VkQueue getGraphicQueue() const noexcept;
		VkQueue getPresnetQueue() const noexcept;
		QueueFamilyIndices getQueueFamilyIndices() const noexcept;
		std::shared_ptr<Surface> GetSurface() const noexcept;
	private:
		bool isDeviceSuitable(VkPhysicalDevice device);
		// pick the best gpu
		void pickPhysicalDevice(VkInstance instance);
		// use specified gpu
		void setPhysicalDevice(u32 device_index);

		void createDevice(const ValidationLayer& validation_layers);

		bool checkDeviceExtensionSupport(VkPhysicalDevice device);


	private:
		u32 device_count;
		i32 m_physical_device_index = -1;
		std::vector<VkPhysicalDevice> m_physical_devices;
		VkDevice m_device{};
		VkQueue m_graphics_queue, m_present_queue, m_compute_queue;
		QueueFamilyIndices m_queue_family_indices;
		std::shared_ptr<Instance> m_instance = nullptr;
		std::shared_ptr<Surface> m_surface = nullptr;
		const std::vector<const char*> m_device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME };
	};

	class VulkanDevice{
		public:
		
	};
}