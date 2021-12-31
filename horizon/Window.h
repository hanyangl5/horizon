#pragma once
#include <cstdint>
#include <memory>
#include "utils.h"


class Window {
public:
	Window(const char* _name, u32 _width, u32 _height);
	~Window();
	u32 getWidth()const;
	u32 getHeight()const;
	GLFWwindow* getWindow();
private:
	GLFWwindow* mWindow;
	u32 width, height;
};