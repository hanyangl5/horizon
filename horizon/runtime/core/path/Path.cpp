#include <filesystem>

#include "Path.h"

namespace Horizon {

	std::string Path::GetAssetsPath() const noexcept
	{
		// ../../assets
		return std::filesystem::current_path().parent_path().string().append("/assets");
	}

	std::string Path::GetModelPath(std::string _path) const noexcept
	{
		return GetAssetsPath().append("/models/").append(_path);
	}

	std::string Path::GetShaderPath(std::string _path) const noexcept
	{
		return GetAssetsPath().append("/shaders/spirv/").append(_path);
	}

}