#pragma once
#include <cstdint>
#include <memory>

#include <runtime/core/math/Math.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

namespace Horizon {

class Window {
public:
	Window(const char* _name, u32 _width, u32 _height) noexcept;
	~Window()noexcept;
	u32 getWidth()const noexcept;
	u32 getHeight()const noexcept;
	GLFWwindow* getWindow() const noexcept;
	int ShouldClose() const noexcept;
	void close()noexcept;
private:
	GLFWwindow* m_window;
	u32 width, height;
	bool vsync_enabled = false;
};
}