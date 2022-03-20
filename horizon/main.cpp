#include <iostream>
#include <thread>
#include <chrono>

#include "utils.h"
#include "Window.h"
#include "App.h"

using namespace Horizon;

const u32 w = 1920, h = 1080;

int main(int argc, char* argv[])
{

	App app(w, h);

	try {
		app.run();
	}
	catch (const std::exception& e) {
		spdlog::error(e.what());
		return 1;
	}

	return 0;
}
