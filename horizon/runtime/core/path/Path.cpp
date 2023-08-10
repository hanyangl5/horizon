#include "Path.h"

#include <filesystem>

#include <config.hpp>

namespace Horizon {

std::string Path::GetAssetsPath() const noexcept {
    return {ASSET_DIR};
}

std::string Path::GetModelPath(const std::string& _path) const noexcept {
    return GetAssetsPath().append("/models/").append(_path);
}

std::string Path::GetShaderPath(const std::string &_path) const noexcept {
    return GetAssetsPath().append("/shaders/spirv/").append(_path);
}
std::string Path::GetTexturePath(const std::string &_path) const noexcept {
    return GetAssetsPath().append("/textures/").append(_path);
}
} // namespace Horizon