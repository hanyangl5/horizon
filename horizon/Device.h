#pragma once

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Instance.h"
#include "Surface.h"
#include "QueueFamilyIndices.h"
#include "ValidationLayer.h"

namespace Horizon {

	class Device
	{
	public:
		Device(Instance* instance, Surface* surface);
		~Device();
		VkPhysicalDevice getPhysicalDevice() const;
		VkDevice get()const;
		VkQueue getGraphicQueue()const;
		VkQueue getPresnetQueue()const;
		QueueFamilyIndices getQueueFamilyIndices();
	private:
		bool isDeviceSuitable(VkPhysicalDevice device);
		// pick the best gpu
		void pickPhysicalDevice(VkInstance instance);
		// use specified gpu
		void setPhysicalDevice(u32 deviceIndex);

		void createDevice(const ValidationLayer& validationLayers);

		bool checkDeviceExtensionSupport(VkPhysicalDevice device);


	private:
		u32 deviceCount;
		i32 mPhysicalDeviceIndex = -1;
		std::vector<VkPhysicalDevice> mPhysicalDevices;
		VkDevice mDevice{};
		VkQueue mGraphicsQueue, mPresentQueue;
		QueueFamilyIndices mQueueFamilyIndices;
		Instance* mInstance = nullptr;
		Surface* mSurface = nullptr;
		const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	};

}