#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
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
	main_window = glfwCreateWindow(_width, _height, "Dredgen", nullptr, nullptr);

	if (!main_window)
	{
		glfwTerminate();
		spdlog::error("failed to init window");
	}
}
Window::~Window()
{
	glfwDestroyWindow(main_window);
	glfwTerminate();
	spdlog::info("window destroyed");
}

void Window::Run()
{
	renderer = std::make_unique<Renderer>(width, height, main_window);

	while (!glfwWindowShouldClose(main_window))
	{
		glfwPollEvents();
		renderer->Update();
		renderer->Render();
	}
}