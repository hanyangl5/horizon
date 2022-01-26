#include "App.h"
#include "spdlog/sinks/stdout_color_sinks.h" // or "../stdout_sinks.h" if no colors needed
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
	delete mRenderer;
	delete mWindow;
}



void App::run() {

	mWindow = new Window("horizon", mWidth, mHeight);
	mRenderer = new Renderer(mWindow->getWidth(), mWindow->getHeight(), mWindow);

	while (!glfwWindowShouldClose(mWindow->getWindow()))
	{
		glfwPollEvents();
		mRenderer->Update();
		mRenderer->Render();
	}
	mRenderer->wait();
}