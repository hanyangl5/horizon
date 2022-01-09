#include <cstdint>
#include "utils.h"
#include "Window.h"

Window::Window(const char* _name, u32 _width, u32 _height) : width(_width), height(_height)
{
	if (glfwInit() != GLFW_TRUE)
	{
		glfwTerminate();
		spdlog::error("failed to init glfw");
	};
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	mWindow = glfwCreateWindow(_width, _height, _name, nullptr, nullptr);

	if (!mWindow)
	{
		glfwTerminate();
		spdlog::error("failed to init window");
	}
}
Window::~Window()
{
	glfwDestroyWindow(mWindow);
	glfwTerminate();
	//spdlog::info("window destroyed");
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
	return mWindow;
}
