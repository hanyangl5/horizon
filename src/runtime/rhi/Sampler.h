#pragma once

#include <runtime/rhi/Enums.h>

namespace Horizon::Backend {
class Sampler {
  public:
    Sampler() noexcept {};
    virtual ~Sampler() noexcept = default;

    Sampler(const Sampler &rhs) noexcept = delete;
    Sampler &operator=(const Sampler &rhs) noexcept = delete;
    Sampler(Sampler &&rhs) noexcept = delete;
    Sampler &operator=(Sampler &&rhs) noexcept = delete;

  public:
};

} // namespace Horizon::Backend