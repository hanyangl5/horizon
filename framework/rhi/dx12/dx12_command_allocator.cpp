#include "dx12_command_allocator.h"
#include <core/log.h>

namespace Horizon::Backend
{

DX12CommandAllocatorPool::DX12CommandAllocatorPool(const DX12RendererContext &context,
                                                   CommandQueueType queue_type) noexcept
    : m_context(context), m_queue_type(queue_type)
{
    // Pre-allocate a few allocators
    m_allocators.reserve(4);
    for (u32 i = 0; i < 4; ++i)
    {
        ComPtr<ID3D12CommandAllocator> allocator;
        D3D12_COMMAND_LIST_TYPE list_type = ToDX12CommandListType(queue_type);
        HRESULT hr = m_context.device->CreateCommandAllocator(list_type, IID_PPV_ARGS(&allocator));
        if (SUCCEEDED(hr))
        {
            m_allocators.push_back(allocator);
            m_available_allocators.push_back(allocator);
        }
        else
        {
            LOG_ERROR("Failed to create command allocator: {}", hr);
        }
    }
}

DX12CommandAllocatorPool::~DX12CommandAllocatorPool() noexcept
{
    // ComPtr will automatically release
}

ComPtr<ID3D12CommandAllocator> DX12CommandAllocatorPool::GetAllocator()
{
    if (!m_available_allocators.empty())
    {
        auto allocator = m_available_allocators.back();
        m_available_allocators.pop_back();
        return allocator;
    }

    // Create a new allocator if none available
    ComPtr<ID3D12CommandAllocator> allocator;
    D3D12_COMMAND_LIST_TYPE list_type = Horizon::ToDX12CommandListType(m_queue_type);
    HRESULT hr = m_context.device->CreateCommandAllocator(list_type, IID_PPV_ARGS(&allocator));
    if (SUCCEEDED(hr))
    {
        m_allocators.push_back(allocator);
        return allocator;
    }
    else
    {
        LOG_ERROR("Failed to create command allocator: {}", hr);
        return nullptr;
    }
}

void DX12CommandAllocatorPool::ResetAllocator(ComPtr<ID3D12CommandAllocator> allocator)
{
    if (allocator != nullptr)
    {
        HRESULT hr = allocator->Reset();
        if (SUCCEEDED(hr))
        {
            m_available_allocators.push_back(allocator);
        }
        else
        {
            LOG_ERROR("Failed to reset command allocator: {}", hr);
        }
    }
}

void DX12CommandAllocatorPool::ResetAll()
{
    for (auto &allocator : m_allocators)
    {
        if (allocator != nullptr)
        {
            HRESULT hr = allocator->Reset();
            if (SUCCEEDED(hr))
            {
                // Add back to available list if not already there
                bool found = false;
                for (const auto &avail : m_available_allocators)
                {
                    if (avail.Get() == allocator.Get())
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    m_available_allocators.push_back(allocator);
                }
            }
        }
    }
}

} // namespace Horizon::Backend
