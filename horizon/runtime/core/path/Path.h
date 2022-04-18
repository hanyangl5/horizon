#pragma once

#include <string>

#include <runtime/core/singleton/public_singleton.h>

namespace Horizon {
	class Path :public PublicSingleton<Path>
	{
	private:
		std::string GetAssetsPath();
	public:
		std::string GetModelPath(std::string _path);
		std::string GetShaderPath(std::string _path);
	};
}

