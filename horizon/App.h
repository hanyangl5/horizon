#pragma once

#include <memory>
#include <spdlog/spdlog.h>

#include "utils.h"
#include "Window.h"
#include "Renderer.h"
#include "InputManager.h"
class App
{
public:
	App(Horizon::u32 width, Horizon::u32 height);
	~App();
	void run();
private:
	Horizon::u32 mWidth;
	Horizon::u32 mHeight;
	Horizon::Window* mWindow = nullptr;
	Horizon::Renderer* mRenderer = nullptr;
	Horizon::InputManager inputManager;
	std::shared_ptr<spdlog::logger> mLogger;
};
