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
	std::shared_ptr<Horizon::Window> mWindow = nullptr;
	std::unique_ptr<Horizon::Renderer> mRenderer = nullptr;
	std::unique_ptr<Horizon::InputManager> inputManager;
	std::shared_ptr<spdlog::logger> mLogger;
};
