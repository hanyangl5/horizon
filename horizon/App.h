#pragma once
#include "Renderer.h"
#include "Window.h"
#include "utils.h"
#include <memory>
#include <spdlog/spdlog.h>
class App
{
public:
	App(u32 width, u32 height);
	~App();
	void run();
private:
	u32 mWidth;
	u32 mHeight;
	Window* mWindow = nullptr;
	Renderer* mRenderer = nullptr;

	std::shared_ptr<spdlog::logger> mLogger;
};
