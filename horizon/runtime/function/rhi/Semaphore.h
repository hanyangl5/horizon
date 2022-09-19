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

  public:
    virtual void AddWaitStage(CommandQueueType queue_type) noexcept = 0;

    // TODO: how to deal with semaphore signal/wait multiple times, how flags change.
    virtual u32 GetWaitStage() noexcept = 0;

  protected:
    u32 flags{};
};

} // namespace Horizon::RHI