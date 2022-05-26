#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace Horizon {
	class SurfaceSupportDetails
	{
	public:
		SurfaceSupportDetails() = default;

		SurfaceSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface);

		VkSurfaceCapabilitiesKHR getCapabilities() const noexcept;

		std::vector<VkSurfaceFormatKHR> getFormats() const noexcept;

		std::vector<VkPresentModeKHR> getPresentModes() const noexcept;

		bool suitable() const noexcept;

	private:
		VkSurfaceCapabilitiesKHR capabilities;

		std::vector<VkSurfaceFormatKHR> formats;

		std::vector<VkPresentModeKHR> presentModes;

	};

}
