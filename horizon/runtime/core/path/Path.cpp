#include <filesystem>

#include "Path.h"

namespace Horizon {

std::string Path::GetAssetsPath() const noexcept { return {}; }

std::string Path::GetModelPath(std::string _path) const noexcept {
    return {};
}

std::string Path::GetShaderPath(std::string _path) const noexcept { return {}; }

} // namespace Horizon