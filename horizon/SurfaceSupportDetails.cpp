#include "SurfaceSupportDetails.h"
#include "utils.h"


SurfaceSupportDetails::SurfaceSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	printVkError(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities),"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

	u32 formatCount=0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);  // get count
	if (formatCount == 0)
		spdlog::error("vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats");
	formats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());  // get available surface formats

	u32 presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount == 0)
		spdlog::error("vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats");
	presentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());

}

VkSurfaceCapabilitiesKHR SurfaceSupportDetails::getCapabilities() const
{
	return capabilities;
}

std::vector<VkSurfaceFormatKHR> SurfaceSupportDetails::getFormats() const
{
	return formats;
}

std::vector<VkPresentModeKHR> SurfaceSupportDetails::getPresentModes() const
{
	return presentModes;
}

bool SurfaceSupportDetails::suitable() const
{
	return !formats.empty() && !presentModes.empty();
}
