#include "QueueFamily.h"
#include <vector>


QueueFamilyIndices::QueueFamilyIndices(VkPhysicalDevice device)
{
	u32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr); // get count
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data()); // get queue family properties

	// loop all queue families
	for (u32 i = 0; i < queueFamilyCount; i++)
	{
		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			graphics = i;
		}

		//VkBool32 presentSupport = false;
		//vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		//if (queue_family_properties[i].queueCount > 0 && presentSupport)
		//{
		//	present = i;
		//}

		if (completed())
		{
			break;
		}
	}
}

bool QueueFamilyIndices::completed() const
{
	//return graphics >= 0 && present >= 0;
	return graphics >= 0;
}

i32 QueueFamilyIndices::getGraphics() const
{
	return graphics >= 0 ? graphics : -1;
}

i32 QueueFamilyIndices::getPresent() const
{
	return present >= 0 ? present : -1;
}
