#include "dx12_command_context.h"
#include "dx12_command_list.h"
#include <core/log.h>
#include <core/memory.h>

namespace Horizon::Backend
{

DX12CommandContext::DX12CommandContext(const DX12RendererContext &context) noexcept : m_context(context)
{
    m_command_lists_count.fill(0);

    // Create allocator pools for each queue type
    for (u32 i = 0; i < 3; ++i)
    {
        m_allocator_pools[i] = std::make_unique<DX12CommandAllocatorPool>(context, static_cast<CommandQueueType>(i));
    }
}

DX12CommandContext::~DX12CommandContext() noexcept
{
    // Free all command lists
    for (auto &cls : m_command_lists)
    {
        for (auto &cl : cls)
        {
            Memory::Free(cl);
            cl = nullptr;
        }
    }
}

CommandList *DX12CommandContext::GetCommandList(CommandQueueType type)
{
    u32 index = static_cast<u32>(type);
    u32 count = m_command_lists_count[index];

    if (count >= m_command_lists[index].size())
    {
        // Create a new command list
        ComPtr<ID3D12CommandAllocator> allocator = m_allocator_pools[index]->GetAllocator();
        if (allocator == nullptr)
        {
            LOG_ERROR("Failed to get command allocator");
            return nullptr;
        }

        ComPtr<ID3D12GraphicsCommandList> command_list;
        D3D12_COMMAND_LIST_TYPE list_type = Horizon::ToDX12CommandListType(type);
        HRESULT hr =
            m_context.device->CreateCommandList(0, list_type, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create command list: {}", hr);
            return nullptr;
        }

        // Command lists are created in recording state, close it immediately
        command_list->Close();

        m_command_lists[index].emplace_back(Memory::Alloc<DX12CommandList>(m_context, type, command_list, allocator));
    }

    m_command_lists_count[index]++;
    return m_command_lists[index][count];
}

void DX12CommandContext::Reset()
{
    // Reset all allocator pools
    for (auto &pool : m_allocator_pools)
    {
        if (pool)
        {
            pool->ResetAll();
        }
    }

    // Reset command list counts
    m_command_lists_count.fill(0);
}

} // namespace Horizon::Backend
