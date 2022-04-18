#include "Window.h"

#include <runtime/core/log/Log.h>

namespace Horizon {

	Window::Window(const char* _name, u32 _width, u32 _height) : width(_width), height(_height)
	{
		if (glfwInit() != GLFW_TRUE)
		{
			glfwTerminate();
			LOG_ERROR("failed to init glfw");
		};
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		m_window = glfwCreateWindow(_width, _height, _name, nullptr, nullptr);

		if (!m_window)
		{
			glfwTerminate();
			LOG_ERROR("failed to init window");
		}
	}
	Window::~Window()
	{
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	u32 Window::getWidth() const
	{
		return width;
	}

	u32 Window::getHeight() const
	{
		return height;
	}


	GLFWwindow* Window::getWindow()
	{
		return m_window;
	}

	void Window::close()
	{
		glfwSetWindowShouldClose(m_window, true);
	}

}