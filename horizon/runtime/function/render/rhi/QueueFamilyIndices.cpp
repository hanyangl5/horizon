#include "QueueFamilyIndices.h"

#include <vector>

#include <runtime/core/log/Log.h>

namespace Horizon {

	QueueFamilyIndices::QueueFamilyIndices()
	{
		graphics = false;
		present = false;
	}

	QueueFamilyIndices::QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		u32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr); // get count
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data()); // get queue family properties

		// print queue infos
		for (const auto& i : queueFamilies) {
			std::string queueFlagsStr = "";

			if (i.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				queueFlagsStr += "VK_QUEUE_GRAPHICS_BIT|";
			if (i.queueFlags & VK_QUEUE_COMPUTE_BIT)
				queueFlagsStr += "VK_QUEUE_COMPUTE_BIT|";
			if (i.queueFlags & VK_QUEUE_TRANSFER_BIT)
				queueFlagsStr += "VK_QUEUE_TRANSFER_BIT|";
			if (i.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
				queueFlagsStr += "VK_QUEUE_SPARSE_BINDING_BIT|";
			if (i.queueFlags & VK_QUEUE_PROTECTED_BIT)
				queueFlagsStr += "VK_QUEUE_PROTECTED_BIT";

			LOG_DEBUG(" queueinfo :queueCount:{} queueFlags:{}", i.queueCount, queueFlagsStr);
		}

		// loop all queue families
		for (u32 i = 0; i < queueFamilyCount; i++)
		{
			// queue support graphics operation
			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				graphics = i;
			}

			// queue support present operation
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (queueFamilies[i].queueCount > 0 && presentSupport)
			{
				present = i;
			}

			if (completed())
			{
				break;
			}
		}
	}

	bool QueueFamilyIndices::completed() const
	{
		return graphics.has_value() && present.has_value();
	}

	u32 QueueFamilyIndices::getGraphics() const
	{
		return graphics.value();
	}

	u32 QueueFamilyIndices::getPresent() const
	{
		return present.value();
	}
}