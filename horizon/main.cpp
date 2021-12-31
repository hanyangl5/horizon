#include <iostream>
#include "utils.h"
#include "Window.h"
#include <chrono>
#include <thread>
#include "App.h"
const u32 w = 1280, h = 720;

int main(int argc, char* argv[])
{

	App app(w,h);

	try {
		app.run();
	}
	catch (const std::exception& e) {
		spdlog::error( e.what());
		return 1;
	}

	return 0;
}
