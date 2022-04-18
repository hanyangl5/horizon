#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <runtime/core/utils/utils.h>
#include <runtime/function/window/Window.h>
#include <runtime/scene/render/Renderer.h>
#include <runtime/function/input/InputManager.h>

class App
{
public:
	App(Horizon::u32 _width, Horizon::u32 _height);
	~App();
	void run();
private:
	Horizon::u32 m_width;
	Horizon::u32 mHeight;
	std::shared_ptr<Horizon::Window> m_window = nullptr;
	std::unique_ptr<Horizon::Renderer> m_renderer = nullptr;
	std::unique_ptr<Horizon::InputManager> m_input_manager;
	std::shared_ptr<spdlog::logger> m_logger;
};
