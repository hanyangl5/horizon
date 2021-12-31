#include "App.h"

App::App(u32 width, u32 height) :mWidth(width), mHeight(height)
{
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

	while (!glfwWindowShouldClose(mWindow->getWindow()))
	{
		glfwPollEvents();
		mRenderer->Update();
		mRenderer->Render();
	}
}