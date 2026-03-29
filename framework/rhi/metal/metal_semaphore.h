#pragma once

#include <rhi/semaphore.h>

namespace Horizon::Backend
{

class MetalSemaphore final : public Semaphore
{
  public:
    void AddWaitStage(CommandQueueType queue_type) noexcept override;
    u32 GetWaitStage() noexcept override;
};

} // namespace Horizon::Backend
