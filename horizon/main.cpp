#include <iostream>
#include <spdlog/spdlog.h>
#include "utils.h"
#include "Window.h"
#include <chrono>
#include <thread>
const u32 w = 1280, h = 720;

int main(int argc, char* argv[])
{

	Window window{ "dredgen graphic engine",w,h };

	try {
		window.Run();
	}
	catch (const std::exception& e) {
		spdlog::error( e.what());
		return 1;
	}

	return 0;
}
