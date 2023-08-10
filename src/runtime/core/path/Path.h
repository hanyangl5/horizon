#pragma once

#include <string>

#include <runtime/core/singleton/public_singleton.h>

namespace Horizon::Path {
static std::string GetAssetsPath() noexcept;

std::string GetModelPath(const std::string &_path) noexcept;
std::string GetTexturePath(const std::string &_path) noexcept;
std::string GetShaderPath(const std::string &_path) noexcept;
} // namespace Horizon::Path
