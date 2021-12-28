#include "App.h"




App::App(u32 width, u32 height) :mWidth(width), mHeight(height)
{

}

App::~App()
{
}



void App::run() {
	mWindow = std::make_unique<Window>("horizon", mWidth, mHeight);
	mRenderer = std::make_unique<Renderer>(mWindow->getWidth(), mWindow->getHeight(), mWindow->getWindow());

	while (!glfwWindowShouldClose(mWindow->getWindow()))
	{
		glfwPollEvents();
		mRenderer->Update();
		mRenderer->Render();
	}
}