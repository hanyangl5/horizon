#pragma once

#include <vulkan/vulkan.hpp>

#include "Instance.h"
#include "utils.h"
#include "Window.h"

namespace Horizon {

	class Surface
	{
	public:
		Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window);
		~Surface();
		VkSurfaceKHR get()const;
	private:
		void createSurface();
	private:
		std::shared_ptr<Instance> m_instance = nullptr;
		std::shared_ptr<Window> m_window = nullptr;
		VkSurfaceKHR m_surface;
	};

}