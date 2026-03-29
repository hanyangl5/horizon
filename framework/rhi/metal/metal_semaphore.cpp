#include "metal_semaphore.h"

namespace Horizon::Backend
{

void MetalSemaphore::AddWaitStage(CommandQueueType queue_type) noexcept
{
    flags |= 1u << static_cast<u32>(queue_type);
}

u32 MetalSemaphore::GetWaitStage() noexcept
{
    return flags;
}

} // namespace Horizon::Backend
