#pragma once

#include <string>

#include <runtime/core/singleton/public_singleton.h>

namespace Horizon {
class Path : public PublicSingleton<Path> {
  private:
    std::string GetAssetsPath() const noexcept;

  public:
    std::string GetModelPath(std::string _path) const noexcept;
    std::string GetShaderPath(std::string _path) const noexcept;
};
} // namespace Horizon
