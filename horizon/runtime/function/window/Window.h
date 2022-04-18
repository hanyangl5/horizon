#pragma once
#include <cstdint>
#include <memory>
#include <runtime/core/utils/utils.h>
namespace Horizon {

class Window {
public:
	Window(const char* _name, u32 _width, u32 _height);
	~Window();
	u32 getWidth()const;
	u32 getHeight()const;
	GLFWwindow* getWindow();
	void close();
private:
	GLFWwindow* m_window;
	u32 width, height;
};
}