#include "Surface.h"
#include "utils.h"
#include "Window.h"

namespace Horizon {

	Surface::Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window) :m_instance(instance), m_window(window)
	{
		createSurface();
	}

	Surface::~Surface()
	{
		vkDestroySurfaceKHR(m_instance->get(), m_surface, nullptr);
	}

	VkSurfaceKHR Surface::get() const
	{
		return m_surface;
	}

	void Surface::createSurface()
	{
		printVkError(glfwCreateWindowSurface(m_instance->get(), m_window->getWindow(), nullptr, &m_surface), "create surface");
	}
}