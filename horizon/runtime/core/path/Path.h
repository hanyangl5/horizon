#pragma once

#include <string>

#include <runtime/core/singleton/public_singleton.h>

namespace Horizon {
	class Path :public PublicSingleton<Path>
	{
	private:
		std::string GetAssetsPath() const noexcept;
	public:
		std::string GetModelPath(const std::string& _path) const noexcept;
          std::string GetTexturePath(const std::string &_path) const noexcept;
                std::string GetShaderPath(const std::string &_path) const noexcept;
	};
}

