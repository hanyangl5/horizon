#include "Renderer.h"
#include <spdlog/spdlog.h>
#include <iostream>

Renderer::Renderer(int width, int height, GLFWwindow* window)
{
	mCamera = std::make_unique<Camera>();
	const char** t;
	{
		const char** tglfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		//SPDLOG_LOGGER_INFO(g_pLogger, "&nP={}", fmt::ptr(&nP));
		t = tglfwExtension;
		for (int i = 0; i < glfwExtensionCount; i++) {
			glfwExtensions.push_back(*tglfwExtension++);
		}
	}

	spdlog::info(fmt::format("{}", fmt::ptr(t)));

	glfwExtensions;

	mInstance = std::make_unique<Instance>(glfwExtensionCount, glfwExtensions);
}

Renderer::~Renderer()
{

}

void Renderer::Update() {
	// clean up 
}
void Renderer::Render() {

}
