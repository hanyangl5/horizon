#pragma once
#include <vulkan/vulkan.hpp>
#include "Instance.h"
#include "utils.h"
class Surface
{
public:
	Surface(std::shared_ptr<Instance> instance, GLFWwindow * window);
	~Surface();
	VkSurfaceKHR get()const;
private:
	std::shared_ptr<Instance> mInstance;
	VkSurfaceKHR mSurface;
};

