#pragma once

#include <vulkan/vulkan.hpp>

#include <runtime/function/render/RenderContext.h>
#include "Instance.h"
#include "Surface.h"
#include "QueueFamilyIndices.h"
#include "ValidationLayer.h"

namespace Horizon {

	class Device
	{
	public:
		Device(std::shared_ptr<Instance> instance, std::shared_ptr<Surface> surface);
		~Device();
		VkPhysicalDevice getPhysicalDevice() const;
		VkDevice Get()const;
		VkQueue getGraphicQueue()const;
		VkQueue getPresnetQueue()const;
		QueueFamilyIndices getQueueFamilyIndices();
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
		VkQueue m_graphics_queue, m_present_queue;
		QueueFamilyIndices m_queue_family_indices;
		std::shared_ptr<Instance> m_instance = nullptr;
		std::shared_ptr<Surface> m_surface = nullptr;
		const std::vector<const char*> m_device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME };
	};

}