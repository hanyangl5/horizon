#include "SurfaceSupportDetails.h"

#include <runtime/function/render/RenderContext.h>

#include <runtime/core/log/Log.h>

namespace Horizon {

	SurfaceSupportDetails::SurfaceSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities));

		u32 formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);  // Get count
		if (formatCount == 0)
			LOG_ERROR("vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats");
		formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());  // Get available surface formats

		u32 presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount == 0)
			LOG_ERROR("vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats");
		presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());

	}

	VkSurfaceCapabilitiesKHR SurfaceSupportDetails::getCapabilities() const noexcept 
	{
		return capabilities;
	}

	std::vector<VkSurfaceFormatKHR> SurfaceSupportDetails::getFormats() const noexcept 
	{
		return formats;
	}

	std::vector<VkPresentModeKHR> SurfaceSupportDetails::getPresentModes() const noexcept 
	{
		return presentModes;
	}

	bool SurfaceSupportDetails::suitable() const noexcept 
	{
		return !formats.empty() && !presentModes.empty();
	}
}