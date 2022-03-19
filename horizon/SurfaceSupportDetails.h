#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

namespace Horizon {
	class SurfaceSupportDetails
	{
	public:
		SurfaceSupportDetails() = default;

		SurfaceSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface);

		VkSurfaceCapabilitiesKHR getCapabilities() const;

		std::vector<VkSurfaceFormatKHR> getFormats() const;

		std::vector<VkPresentModeKHR> getPresentModes() const;

		bool suitable() const;

	private:
		VkSurfaceCapabilitiesKHR capabilities;

		std::vector<VkSurfaceFormatKHR> formats;

		std::vector<VkPresentModeKHR> presentModes;

	};

}
