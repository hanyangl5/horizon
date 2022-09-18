#pragma once

#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {
class Semaphore {
  public:
    Semaphore() noexcept {};
    virtual ~Semaphore() noexcept = default;

    Semaphore(const Semaphore &rhs) noexcept = delete;
    Semaphore &operator=(const Semaphore &rhs) noexcept = delete;
    Semaphore(Semaphore &&rhs) noexcept = delete;
    Semaphore &operator=(Semaphore &&rhs) noexcept = delete;
  private:
      u32 flags{};
};

} // namespace Horizon::RHI