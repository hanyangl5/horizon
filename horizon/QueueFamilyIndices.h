#pragma once
#include <vulkan/vulkan.hpp>
#include "utils.h"
#include <optional>
class QueueFamilyIndices
{

public:
	QueueFamilyIndices(VkPhysicalDevice device,VkSurfaceKHR surface);

	~QueueFamilyIndices() = default;

	// try to find required queue families and save they indices

	// this device have all required queue families (for this surface)
	bool completed() const;

	u32 getGraphics()const;

	u32 getPresent()const;

private:
	std::optional<u32> graphics;
	std::optional<u32> present;
};

