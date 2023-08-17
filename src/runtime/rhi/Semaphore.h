#pragma once

#include <runtime/rhi/Enums.h>

namespace Horizon::Backend {
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

    // TODO(hylu): how to deal with semaphore signal/wait multiple times, how flags change.
    virtual u32 GetWaitStage() noexcept = 0;

  protected:
    u32 flags{};
};

} // namespace Horizon::Backend