#include "App.h"
#include <memory>

using namespace Horizon;

App::App(u32 _width, u32 _height) :m_width(_width), mHeight(_height)
{

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

int main(int argc, char* argv[])
{
	std::unique_ptr<App> app = std::make_unique<App>(1920, 1080);

	app->run();

	return 0;
}
