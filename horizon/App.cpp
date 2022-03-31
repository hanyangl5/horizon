#include "App.h"
#include "spdlog/sinks/stdout_color_sinks.h" // or "../stdout_sinks.h" if no colors needed

using namespace Horizon;

App::App(u32 width, u32 height) :mWidth(width), mHeight(height), mLogger(spdlog::stdout_color_mt("horizon logger"))
{

	spdlog::set_default_logger(mLogger);
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

	mWindow = std::make_shared<Window>("horizon", mWidth, mHeight);
	mRenderer = std::make_unique<Renderer>(mWindow->getWidth(), mWindow->getHeight(), mWindow);
	inputManager = std::make_unique<InputManager>(mWindow, mRenderer->getMainCamera());

	while (!glfwWindowShouldClose(mWindow->getWindow()))
	{
		glfwPollEvents();
		inputManager->processInput();
		mRenderer->Update();
		mRenderer->Render();
	}
	mRenderer->wait();
}