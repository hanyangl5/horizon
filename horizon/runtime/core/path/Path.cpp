#include "Path.h"

#include <filesystem>

namespace Horizon {

	std::string Path::GetAssetsPath()
	{
		// ../../assets
		return std::filesystem::current_path().parent_path().string().append("/assets");
	}

	std::string Path::GetModelPath(std::string _path)
	{
		return GetAssetsPath().append("/models/").append(_path);
	}

	std::string Path::GetShaderPath(std::string _path)
	{
		return GetAssetsPath().append("/shaders/spirv/").append(_path);
	}

}