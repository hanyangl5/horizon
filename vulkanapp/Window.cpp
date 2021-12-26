#include <cstdint>
#include "utils.h"
#include "Window.h"

Window::Window(const char* _name, uint32_t _width, uint32_t _height) : width(_width), height(_height)
{
	if (glfwInit() != GLFW_TRUE)
	{
		glfwTerminate();
		spdlog::error("failed to init glfw");
	};
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	mWindow = glfwCreateWindow(_width, _height, "Dredgen", nullptr, nullptr);

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
	spdlog::info("window destroyed");
}

void Window::Run()
{
	renderer = std::make_unique<Renderer>(width, height, mWindow);

	while (!glfwWindowShouldClose(mWindow))
	{
		glfwPollEvents();
		renderer->Update();
		renderer->Render();
	}
}