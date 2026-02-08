#pragma once

#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/enums.h>
#include <rhi/semaphore.h>

using Microsoft::WRL::ComPtr;

namespace Horizon::Backend
{

class DX12Semaphore : public Semaphore
{
  public:
    DX12Semaphore(const DX12RendererContext &context) noexcept;
    virtual ~DX12Semaphore() noexcept;

    DX12Semaphore(const DX12Semaphore &rhs) noexcept = delete;
    DX12Semaphore &operator=(const DX12Semaphore &rhs) noexcept = delete;
    DX12Semaphore(DX12Semaphore &&rhs) noexcept = delete;
    DX12Semaphore &operator=(DX12Semaphore &&rhs) noexcept = delete;

    void AddWaitStage(CommandQueueType queue_type) noexcept override;
    u32 GetWaitStage() noexcept override;

    ID3D12Fence *GetFence() const noexcept
    {
        return m_fence.Get();
    }

    UINT64 GetFenceValue() const noexcept
    {
        return m_fence_value;
    }

    void SetFenceValue(UINT64 value) noexcept
    {
        m_fence_value = value;
    }

  private:
    const DX12RendererContext &m_context;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fence_value{0};
};

} // namespace Horizon::Backend
