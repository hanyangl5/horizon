#include "dx12_semaphore.h"
#include <core/log.h>

namespace Horizon::Backend
{

DX12Semaphore::DX12Semaphore(const DX12RendererContext &context) noexcept : m_context(context)
{
    HRESULT hr = m_context.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DX12 semaphore fence: {}", hr);
    }
}

DX12Semaphore::~DX12Semaphore() noexcept
{
    // ComPtr will automatically release
}

void DX12Semaphore::AddWaitStage([[maybe_unused]] CommandQueueType queue_type) noexcept
{
    // In DX12, we use fence values instead of pipeline stages
    // The flags field is used to track which queue types are waiting
    flags |= (1u << static_cast<u32>(queue_type));
}

u32 DX12Semaphore::GetWaitStage() noexcept
{
    // Return the flags and reset them
    u32 wait_flags = flags;
    flags = 0;
    return wait_flags;
}

} // namespace Horizon::Backend
