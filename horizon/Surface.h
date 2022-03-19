#pragma once

#include <vulkan/vulkan.hpp>

#include "Instance.h"
#include "utils.h"
#include "Window.h"

namespace Horizon {

	class Surface
	{
	public:
		Surface(Instance* instance, Window* window);
		~Surface();
		VkSurfaceKHR get()const;
	private:
		void createSurface();
	private:
		Instance* mInstance = nullptr;
		Window* mWindow = nullptr;
		VkSurfaceKHR mSurface;
	};

}