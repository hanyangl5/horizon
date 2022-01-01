#pragma once
#include <vulkan/vulkan.hpp>
#include "Instance.h"
#include "utils.h"
#include "Window.h"
class Surface
{
public:
	Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window);
	~Surface();
	VkSurfaceKHR get()const;
private:
	std::shared_ptr<Instance> mInstance;
	VkSurfaceKHR mSurface;
};

