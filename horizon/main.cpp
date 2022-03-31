#include <iostream>
#include <memory>
#include "utils.h"
#include "Window.h"
#include "App.h"

using namespace Horizon;

const u32 width = 1920, height = 1080;

int main(int argc, char* argv[])
{

	std::unique_ptr<App> app = std::make_unique<App>(width, height);

	app->run();

	return 0;
}
