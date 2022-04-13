#include "App.h"
#include "spdlog/sinks/stdout_color_sinks.h" // or "../stdout_sinks.h" if no colors needed

using namespace Horizon;

App::App(u32 _width, u32 _height) :m_width(_width), mHeight(_height), m_logger(spdlog::stdout_color_mt("horizon logger"))
{

	spdlog::set_default_logger(m_logger);
#ifndef NDEBUG
	spdlog::set_level(spdlog::level::debug);
#else
	spdlog::set_level(spdlog::level::info);
#endif // !NDEBUG

}

App::~App()
{
}

void App::run() {

	m_window = std::make_shared<Window>("horizon", m_width, mHeight);
	m_renderer = std::make_unique<Renderer>(m_window->getWidth(), m_window->getHeight(), m_window);
	m_input_manager = std::make_unique<InputManager>(m_window, m_renderer->getMainCamera());

	while (!glfwWindowShouldClose(m_window->getWindow()))
	{
		glfwPollEvents();
		m_input_manager->processInput();
		m_renderer->Update();
		m_renderer->Render();
	}
	m_renderer->wait();
}