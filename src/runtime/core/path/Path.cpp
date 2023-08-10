#include "Path.h"

#include <filesystem>

#include <config.hpp>

namespace Horizon::Path {

std::string GetAssetsPath() noexcept { return {ASSET_DIR}; }

std::string GetModelPath(const std::string &_path) noexcept { return GetAssetsPath().append("/models/").append(_path); }

std::string GetShaderPath(const std::string &_path) noexcept {
    return GetAssetsPath().append("/shaders/spirv/").append(_path);
}
std::string GetTexturePath(const std::string &_path) noexcept {
    return GetAssetsPath().append("/textures/").append(_path);
}
} // namespace Horizon::Path