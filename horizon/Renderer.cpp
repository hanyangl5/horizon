#include "Renderer.h"
#include <spdlog/spdlog.h>
#include <iostream>

Renderer::Renderer(int width, int height, GLFWwindow* window)
{
	mCamera = std::make_unique<Camera>();
	mInstance = std::make_shared<Instance>();
	mDevice = std::make_unique<Device>(mInstance);
}

Renderer::~Renderer()
{

}

void Renderer::Update() {
	// clean up 
}
void Renderer::Render() {

}
