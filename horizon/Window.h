#pragma once
#include <cstdint>
#include <memory>
#include "Renderer.h"
#include "utils.h"
class Window {
public:
	Window(const char* _name, uint32_t _width, uint32_t _height);
	~Window();
	void Run();
	GLFWwindow* mWindow;
	u32 width, height;
private:
	std::unique_ptr<Renderer> renderer;
};