#pragma once
#include <cstdint>
#include <memory>
#include "Renderer.h"
#include "utils.h"
class Window {
public:
	Window(const char* _name, u32 _width, u32 _height);
	~Window();
	u32 getWidth()const;
	u32 getHeight()const;
	GLFWwindow* getWindow()const;
private:
	GLFWwindow* mWindow;
	u32 width, height;
};