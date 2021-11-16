#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <memory>
#include "Renderer.h"

class Window {
public:
	Window(const char* _name, uint32_t _width, uint32_t _height);
	~Window();
	void Run();
	GLFWwindow* main_window{};
	uint32_t width{}, height{};
private:
	std::unique_ptr<Renderer> renderer{};
};