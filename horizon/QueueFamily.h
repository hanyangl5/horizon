#pragma once
#include <vulkan/vulkan.hpp>
#include "utils.h"

class QueueFamilyIndices
{

public:
	QueueFamilyIndices(VkPhysicalDevice device);

	~QueueFamilyIndices() = default;

	// try to find required queue families and save they indices

	// this device have all required queue families (for this surface)
	bool completed() const;

	i32 getGraphics()const;

	i32 getPresent()const;

private:
	i32 graphics = -1;
	i32 present = -1;

};

